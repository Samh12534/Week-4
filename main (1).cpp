//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"

UnbufferedSerial uartUsb(USBTX, USBRX, 115200);

AnalogIn potentiometer(A0);
AnalogIn lm35(A1);
AnalogIn gasSensor(A2); 
DigitalInOut sirenPin(PE_10);

DigitalIn enterButton(BUTTON1);
DigitalIn aButton(D4);
DigitalIn bButton(D5);
DigitalIn cButton(D6);
DigitalIn dButton(D7);

DigitalOut alarmLed(LED1);
DigitalOut incorrectCodeLed(LED3);
DigitalOut systemBlockedLed(LED2);

//=====[Declaration and initialization of public global variables]=============

bool quit = false;

char receivedChar = '\0';

float lm35Reading = 0.0;
float lm35TempC = 0.0;
float lm35TempF = 0.0;

float potentiometerReading = 0.0;
float potentiometerScaledToC = 0.0;
float potentiometerScaledToF = 0.0;

float gasSensorValue = 0.0;
float gasSensorPPM = 0.0;

bool overTempAlarm = false;
bool gasAlarm = false;
bool alarmActive = false;

int codeSequence[4] = {1, 1, 0, 0};
int buttonsPressed[4] = {0, 0, 0, 0};
int numberOfIncorrectCodes = 0;
int buttonBeingCompared = 0;
bool incorrectCode = false;


//=====[Function prototypes]===================================================

void availableCommands();
void uartTask();
char pcSerialComCharRead();
void pcSerialComStringWrite(const char* str);

float analogReadingScaledWithTheLM35Formula(float analogReading);
float celsiusToFahrenheit(float tempInCelsiusDegrees);
float potentiometerScaledToCelsius(float analogValue);
float potentiometerScaledToFahrenheit(float analogValue);

bool areEqual();
void checkUnlockCode();
void updateAlarms();
void unusedDemoFunction();

//=====[Main function]=========================================================

int main()
{
    sirenPin.mode(OpenDrain);
    sirenPin.input();

    aButton.mode(PullDown);
    bButton.mode(PullDown);
    cButton.mode(PullDown);
    dButton.mode(PullDown);
    enterButton.mode(PullDown);

    availableCommands();

    while (true) {
        lm35Reading = lm35.read();
        lm35TempC = analogReadingScaledWithTheLM35Formula(lm35Reading);
        lm35TempF = celsiusToFahrenheit(lm35TempC);

        potentiometerReading = potentiometer.read();
        potentiometerScaledToC = potentiometerScaledToCelsius(potentiometerReading);
        potentiometerScaledToF = potentiometerScaledToFahrenheit(potentiometerReading);

        gasSensorValue = gasSensor.read();
        gasSensorPPM = gasSensorValue * 5000.0f; 

        gasAlarm = (gasSensorPPM > 1000.0f);
        overTempAlarm = (lm35TempC > 50.0);
        alarmActive = gasAlarm || overTempAlarm;

        char str[200];
        sprintf(str, "LM35: %.2f degC | Pot: %.2f | Gas: %.2f ppm\r\n",
                lm35TempC, potentiometerReading, gasSensorPPM);
        pcSerialComStringWrite(str);

        updateAlarms();
        uartTask();

        unusedDemoFunction();

        ThisThread::sleep_for(500ms);
    }
}

void updateAlarms()
{
    if (alarmActive) {
        if (gasAlarm && overTempAlarm) {
            pcSerialComStringWrite(">>> Gas & Temperature Alarm <<<\r\n");
        } else if (gasAlarm) {
            pcSerialComStringWrite(">>> Gas Alarm <<<\r\n");
        } else if (overTempAlarm) {
            pcSerialComStringWrite(">>> Temperature Alarm <<<\r\n");
        }

        sirenPin.output();
        sirenPin = LOW;
        alarmLed = ON;
    } else {
        sirenPin.input();
        alarmLed = OFF;
    }
}

void checkUnlockCode()
{
    buttonsPressed[0] = aButton;
    buttonsPressed[1] = bButton;
    buttonsPressed[2] = cButton;
    buttonsPressed[3] = dButton;

    if (areEqual()) {
        pcSerialComStringWrite("\r\nCorrect code entered. Alarm deactivated.\r\n");
        alarmActive = false;
        numberOfIncorrectCodes = 0;
        incorrectCodeLed = OFF;
    } else {
        pcSerialComStringWrite("\r\nIncorrect code. Try again.\r\n");
        incorrectCodeLed = ON;
        numberOfIncorrectCodes++;
        if (numberOfIncorrectCodes >= 5) {
            systemBlockedLed = ON;
        }
    }
}

void uartTask()
{
    receivedChar = pcSerialComCharRead();

    if (receivedChar != '\0') {
        char str[150];
        switch (receivedChar) {
            case '1':
                pcSerialComStringWrite(alarmActive ? "Alarm is ACTIVE\r\n" : "Alarm is OFF\r\n");
                break;
            case '2':
                pcSerialComStringWrite(gasAlarm ? "Gas Detected\r\n" : "No Gas\r\n");
                break;
            case '3':
                pcSerialComStringWrite(overTempAlarm ? "Temperature too high\r\n" : "Temperature normal\r\n");
                break;
            case '4':
                checkUnlockCode();
                break;
            case 'e':
            case 'E':
                sprintf(str, "LM35: %.2f degC, Pot scaled to degC: %.2f\r\n", lm35TempC, potentiometerScaledToC);
                pcSerialComStringWrite(str);
                break;
            case 'f':
            case 'F':
                sprintf(str, "LM35: %.2f degF, Pot scaled to degF: %.2f\r\n", lm35TempF, potentiometerScaledToF);
                pcSerialComStringWrite(str);
                break;
            case 'g':
            case 'G':
                sprintf(str, "Gas Quantity: %.2f ppm\r\n", gasSensorPPM);
                pcSerialComStringWrite(str);
                break;
            default:
                availableCommands();
                break;
        }
    }
}

void availableCommands()
{
    pcSerialComStringWrite("Available commands:\r\n");
    pcSerialComStringWrite("1 - Get alarm state\r\n");
    pcSerialComStringWrite("2 - Get gas sensor state\r\n");
    pcSerialComStringWrite("3 - Get temperature state\r\n");
    pcSerialComStringWrite("4 - Try to unlock alarm\r\n");
    pcSerialComStringWrite("E/e - Show LM35 and Pot in Celsius\r\n");
    pcSerialComStringWrite("F/f - Show LM35 and Pot in Fahrenheit\r\n");
    pcSerialComStringWrite("G/g - Check gas sensor reading\r\n\r\n");
}

char pcSerialComCharRead()
{
    char c = '\0';
    if (uartUsb.readable()) {
        uartUsb.read(&c, 1);
    }
    return c;
}

void pcSerialComStringWrite(const char* str)
{
    uartUsb.write(str, strlen(str));
}

float analogReadingScaledWithTheLM35Formula(float analogReading)
{
    return analogReading * 330.0;
}

float celsiusToFahrenheit(float tempInCelsiusDegrees)
{
    return (tempInCelsiusDegrees * 9.0 / 5.0) + 32.0;
}

float potentiometerScaledToCelsius(float analogValue)
{
    return 148.0 * analogValue + 2.0;
}

float potentiometerScaledToFahrenheit(float analogValue)
{
    return celsiusToFahrenheit(potentiometerScaledToCelsius(analogValue));
}

bool areEqual()
{
    for (int i = 0; i < 4; i++) {
        if (codeSequence[i] != buttonsPressed[i]) {
            return false;
        }
    }
    return true;
}