// C Standard library
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

// XDCtools files
#include <xdc/std.h>
#include <xdc/runtime/System.h>

// BIOS Header files
#include <ti/drivers/i2c/I2CCC26XX.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>

// Board Header files
#include "Board.h"
#include "sensors/mpu9250.h"
#include "buzzer.h"

// Function prototypes
void sendToUART(UART_Handle uart, const char* message);
void uartFxn(UART_Handle handle, void *rxBuf, size_t len);
void debugFxn(void);
void sendMorse(char symbol);
void encodeMorse(char *text);

// Morse Code table: A-Z, 0-9
const char* morseCode[] = {
    ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---", "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--..", // A-Z
    "-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----.", // 0-9
};

#define BUFFERLENGTH 80

// State machine
enum stateTamagotchi {
    WAITING = 1, // Waiting for action
    RIGHT,       // Tilted right
    UP,          // Tilted upwards
    TOP,         // Tilted topwards
    BUTTON       // Button pressed
};

enum stateTamagotchi programState = WAITING;

// Task
#define STACKSIZE 2048
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];

// RTOS-variables and PIN configuration
static PIN_Handle buttonHandle;
static PIN_State buttonState;
static PIN_Handle ledHandle;
static PIN_State ledState;
static PIN_Handle buzzerHandle;
static PIN_State buzzerState;

// Declare the uart handle as a global variable
UART_Handle uart;

// Function to send a message via UART
void sendToUART(UART_Handle uart, const char* message) {
    UART_write(uart, message, strlen(message));
}

// Function to send Morse code symbol
void sendMorse(char symbol) {
    switch(symbol) {
        case '.':
            sendToUART(uart, ".\r\n");
            break;
        case '-':
            sendToUART(uart, "-\r\n");
            break;
        case ' ':
            sendToUART(uart, " \r\n");
            break;
    }
}

// Encode the message to Morse code and send it via UART
void encodeMorse(char *text) {
    while (*text) {
        if (*text == ' ') {
            sendToUART(uart, " \r\n");  // Word separator
            sendToUART(uart, " \r\n");  // Add extra space for word separation
        } else {
            // Convert char to uppercase for consistency
            char c = (*text >= 'a' && *text <= 'z') ? *text - 'a' + 'A' : *text;
            if (c >= 'A' && c <= 'Z') {
                sendToUART(uart, morseCode[c - 'A']);
                sendToUART(uart, "\r\n");  // Each character in Morse is separated by a newline
            } else if (c >= '0' && c <= '9') {
                sendToUART(uart, morseCode[c - '0' + 26]);
                sendToUART(uart, "\r\n");
            }
        }
        text++;
    }
    sendToUART(uart, "\r\n\r\n");  // End the message with 3 spaces
}

// UART Task
Void uartTaskFxn(UArg arg0, UArg arg1) {
    // UART connection setup
    UART_Params uartParams;
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_TEXT;
    uartParams.readDataMode = UART_DATA_TEXT;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.readMode = UART_MODE_BLOCKING;
    uartParams.baudRate = 9600;
    uartParams.dataLength = UART_LEN_8;
    uartParams.parityType = UART_PAR_NONE;
    uartParams.stopBits = UART_STOP_ONE;

    // Open UART
    uart = UART_open(Board_UART0, &uartParams);
    if (uart == NULL) {
        System_abort("Error opening the UART");
    }

    while (1) {
        // Morse encoding logic based on the program state
        if (programState == BUTTON) {
            encodeMorse("hello world");  // Example message to send
            programState = WAITING;  // Reset state after sending
        }

        Task_sleep(1000000 / Clock_tickPeriod);  // Task sleep
    }
}

// Sensor Task
Void sensorTaskFxn(UArg arg0, UArg arg1) {
    // Sensor task logic here (accelerometer and button press handling)
    while (1) {
        // Simulating state change based on sensor values (add your sensor logic here)
        if (/* condition based on sensor data */) {
            programState = BUTTON;  // Change to BUTTON state when condition met
        }
        Task_sleep(1000000 / Clock_tickPeriod);  // Task sleep
    }
}

// Main function
void main(void) {
    // Initialize board drivers
    Board_init();

    // Setup UART and GPIO
    uart_init();
    gpio_init();

    // Initialize sensor task and UART task
    Task_create(sensorTaskFxn, sensorTaskStack, NULL);
    Task_create(uartTaskFxn, uartTaskStack, NULL);

    // Start BIOS
    BIOS_start();
}
