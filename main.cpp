//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"

//=====[Declaration and initialization of public global objects]===============

UnbufferedSerial uartUsb(USBTX, USBRX, 115200);

AnalogIn potentiometer(A0);
AnalogIn lm35(A1);           // 10 mV/° C
AnalogIn mq2(A2);
DigitalInOut buzzer(PE_10);

//=====[Declaration and initialization of public global variables]=============

float lm35TempC          = 0.0;   // Temperature reading [° C]
float gasSensorRead      = 0.0;   // Scaled gas sensor reading (0.0–2.0)
float potReading         = 0.0;   // Raw potentiometer reading (0.0–1.0)
float tempThreshold      = 0.0;   // Temperature threshold [° C] — set by potentiometer
float gasThreshold       = 0.0;   // Gas threshold (0.0–2.0)    — set by potentiometer

bool tempWarning         = false;
bool gasWarning          = false;
bool serialPaused        = false;  // 'q' toggles this

//=====[Declarations (prototypes) of public functions]=========================

float readTemperatureCelsius();
void  evaluateSensors();
void  activateBuzzer( const char* cause );
void  deactivateBuzzer();
void  printSystemStatus();
void  handleSerialInput();
void  printMenu();
void  pcSerialComStringWrite( const char* str );
char  pcSerialComCharRead();

//=====[Main function, the program entry point after power on or reset]========

int main()
{
    buzzer.mode(OpenDrain);
    buzzer.input(); // Buzzer off at startup

    printMenu();

    while( true ) {

        // i. Continuously read all three sensors
        // Double-read each channel: first read flushes ADC sample-and-hold
        // crosstalk from the previous channel, second read is the clean value
        lm35.read();          lm35TempC     = readTemperatureCelsius();
        mq2.read();           gasSensorRead = mq2.read() * 2.0;
        potentiometer.read(); potReading    = potentiometer.read();

        // ii. Potentiometer always sets both thresholds
        tempThreshold = potReading * 100.0; // Maps to 0–100 ° C
        gasThreshold  = potReading * 0.55;  // Maps to 0.0–0.55 (matches actual gas sensor range)

        // Check for serial input ('q' pause or 't'/'g' threshold commands)
        handleSerialInput();

        // iii–v. Evaluate sensors, control buzzer, print status (if not paused)
        evaluateSensors();

        delay(500);
    }
}

//=====[Implementations of public functions]===================================

// i. Read LM35 and convert raw ADC to Celsius
float readTemperatureCelsius()
{
    return lm35.read() * 330.0;
}

// iii. Compare sensor readings against thresholds and trigger iv / v
void evaluateSensors()
{
    tempWarning = (lm35TempC     > tempThreshold);
    gasWarning  = (gasSensorRead > gasThreshold);

    if (tempWarning && gasWarning) {
        activateBuzzer("Temperature and Gas");

    } else if (tempWarning) {
        activateBuzzer("Temperature");

    } else if (gasWarning) {
        activateBuzzer("Gas");

    } else {
        deactivateBuzzer();
        if (!serialPaused) {
            printSystemStatus();
        }
    }
}

// iv. Activate buzzer and send cause to serial monitor (always shown, even when paused)
void activateBuzzer( const char* cause )
{
    buzzer.output();
    buzzer = 0; // Drive low = buzzer ON (open-drain)

    char str[100] = "";
    sprintf(str, "Buzzer ON - Cause: %s\r\n", cause);
    pcSerialComStringWrite(str); // Warnings always print regardless of pause
}

// v. Deactivate buzzer
void deactivateBuzzer()
{
    buzzer.input(); // High-Z = buzzer OFF (open-drain)
}

// v. Show live readings and "System Normal" on serial terminal
void printSystemStatus()
{
    char str[200] = "";
    sprintf(str,
        "Temp: %.2f C | Gas: %.2f | Temp Threshold: %.1f C | Gas Threshold: %.2f | System Normal\r\n",
        lm35TempC,
        gasSensorRead,
        tempThreshold,
        gasThreshold
    );
    pcSerialComStringWrite(str);
}

// Check for incoming serial commands
void handleSerialInput()
{
    char c = pcSerialComCharRead();
    if (c == '\0') return;

    switch (c) {

        // Toggle serial monitor pause/resume
        case 'q':
        case 'Q':
            serialPaused = !serialPaused;
            if (serialPaused) {
                pcSerialComStringWrite("--- Serial monitor PAUSED. Press 'q' to resume. ---\r\n");
            } else {
                pcSerialComStringWrite("--- Serial monitor RESUMED. ---\r\n");
            }
            break;

        default:
            printMenu();
            break;
    }
}

void printMenu()
{
    pcSerialComStringWrite("\r\n=== Sensor Alarm System ===\r\n");
    pcSerialComStringWrite(" 'q' - Pause / Resume serial monitor\r\n");
    pcSerialComStringWrite(" Thresholds controlled by potentiometer.\r\n");
    pcSerialComStringWrite("===========================\r\n\r\n");
}

void pcSerialComStringWrite( const char* str )
{
    uartUsb.write( str, strlen(str) );
}

char pcSerialComCharRead()
{
    char c = '\0';
    if (uartUsb.readable()) {
        uartUsb.read(&c, 1);
    }
    return c;
}
