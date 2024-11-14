/* C Standard library */
#include <stdio.h>
#include <string.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>

/* Board Header files */
#include "Board.h"
#include "sensors/opt3001.h"
#include "sensors/mpu9250.h"

// Task
#define STACKSIZE 2048
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];

// State machine states
enum state { WAITING=1, DATA_READY, DOT, DASH, SPACE, SOS, MAYDAY };
enum state programState = WAITING;

// Global variables
double ambientLight = -1000.0;
float ax, ay, az, gx, gy, gz;
UART_Handle uart;
I2C_Handle i2c;

// Button and LED configuration
static PIN_Handle buttonHandle;
static PIN_State buttonState;
static PIN_Handle ledHandle;
static PIN_State ledState;
PIN_Config buttonConfig[] = {
    Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    Board_BUTTON1 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE
};
PIN_Config ledConfig[] = {
    Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

static int buttonPressCount = 0;
static Clock_Handle buttonClockHandle;
static Clock_Struct buttonClockStruct;

void buttonClockFxn(UArg arg) {
    if (buttonPressCount == 1) {
        programState = DOT;
    } else if (buttonPressCount == 2) {
        programState = DASH;
    } else if (buttonPressCount == 3) {
        programState = SPACE;
    }
    buttonPressCount = 0;
}

void buttonFxn(PIN_Handle handle, PIN_Id pinId) {
    if (pinId == Board_BUTTON1) {
        buttonPressCount++;
        Clock_start(buttonClockHandle);
    } else if (pinId == Board_BUTTON0) {
        PIN_setOutputValue(ledHandle, Board_LED0, !PIN_getOutputValue(Board_LED0));
    }
}

// Morse code mapping
typedef struct {
    char *code;
    char letter;
} MorseCode;

MorseCode morseMap[] = {
    {".-", 'A'}, {"-...", 'B'}, {"-.-.", 'C'}, {"-..", 'D'}, {".", 'E'},
    {"..-.", 'F'}, {"--.", 'G'}, {"....", 'H'}, {"..", 'I'}, {".---", 'J'},
    {"-.-", 'K'}, {".-..", 'L'}, {"--", 'M'}, {"-.", 'N'}, {"---", 'O'},
    {".--.", 'P'}, {"--.-", 'Q'}, {".-.", 'R'}, {"...", 'S'}, {"-", 'T'},
    {"..-", 'U'}, {"...-", 'V'}, {".--", 'W'}, {"-..-", 'X'}, {"-.--", 'Y'},
    {"--..", 'Z'}, {"-----", '0'}, {".----", '1'}, {"..---", '2'}, {"...--", '3'},
    {"....-", '4'}, {".....", '5'}, {"-....", '6'}, {"--...", '7'}, {"---..", '8'},
    {"----.", '9'}, {NULL, '\0'}
};

char decodeMorse(char *morse) {
    int i;
    for (i = 0; morseMap[i].code != NULL; i++) {
        if (strcmp(morseMap[i].code, morse) == 0) {
            return morseMap[i].letter;
        }
    }
    return '?'; // Unknown symbol
}

Void uartTaskFxn(UArg arg0, UArg arg1) {
    UART_Params uartParams;
    UART_Params_init(&uartParams);
    uartParams.baudRate = 9600;
    uart = UART_open(Board_UART0, &uartParams);
    if (uart == NULL) {
        System_abort("Error opening the UART");
    }

    while (1) {
        if (programState == DOT) {
            char symbol[3] = {'.', '\r', '\n'};
            UART_write(uart, symbol, 3);
            programState = WAITING;
        } else if (programState == DASH) {
            char symbol[3] = {'-', '\r', '\n'};
            UART_write(uart, symbol, 3);
            programState = WAITING;
        } else if (programState == SPACE) {
            char symbol[3] = {' ', '\r', '\n'};
            UART_write(uart, symbol, 3);
            programState = WAITING;
        }

        Task_sleep(100000 / Clock_tickPeriod); // Check every 100 ms
    }
}

Void sensorTaskFxn(UArg arg0, UArg arg1) {
    I2C_Params i2cParams;
    I2C_Params_init(&i2cParams);
    i2c = I2C_open(Board_I2C, &i2cParams);
    if (i2c == NULL) {
        System_abort("Error Initializing I2C\n");
    }

    mpu9250_setup(&i2c);

    while (1) {
        mpu9250_get_data(&i2c, &ax, &ay, &az, &gx, &gy, &gz);

        if (ax > 1.0) {
            programState = DOT;
        } else if (ax < -1.0) {
            programState = DASH;
        } else if (az > 1.0) {
            programState = SPACE;
        } else {
            programState = WAITING;
        }

        if (programState != WAITING) {
            PIN_setOutputValue(ledHandle, Board_LED0, 1);  // LED on

            char symbol[4];  // Buffer size 4 characters (symbol + CR + LF + null-terminator)

            // Send UART message based on state
            if (programState == DOT) {
                snprintf(symbol, sizeof(symbol), ".\r\n"); // Dot
                UART_write(uart, symbol, strlen(symbol));
            } else if (programState == DASH) {
                snprintf(symbol, sizeof(symbol), "-\r\n"); // Dash
                UART_write(uart, symbol, strlen(symbol));
            } else if (programState == SPACE) {
                snprintf(symbol, sizeof(symbol), " \r\n"); // Space
                UART_write(uart, symbol, strlen(symbol));
            }

            Task_sleep(500000 / Clock_tickPeriod);  // 500 ms delay
            PIN_setOutputValue(ledHandle, Board_LED0, 0);  // LED off
        }

        System_printf("ax: %f, ay: %f, az: %f, gx: %f, gy: %f, gz: %f\n", ax, ay, az, gx, gy, gz);
        System_flush();

        Task_sleep(1000000 / Clock_tickPeriod);  // 1 second delay before next read
    }
}

Int main(void) {
    Task_Params sensorTaskParams;
    Task_Params uartTaskParams;
    Clock_Params clockParams;

    Board_initGeneral();
    I2C_init();
    UART_init();

    buttonHandle = PIN_open(&buttonState, buttonConfig);
    if (!buttonHandle) {
        System_abort("Error initializing button pins\n");
    }
    if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0) {
        System_abort("Error registering button callback function");
    }
    ledHandle = PIN_open(&ledState, ledConfig);
    if (!ledHandle) {
        System_abort("Error initializing LED pins\n");
    }

    Clock_Params_init(&clockParams);
    clockParams.period = 0;
    clockParams.startFlag = FALSE;
    Clock_construct(&buttonClockStruct, (Clock_FuncPtr)buttonClockFxn, 500000 / Clock_tickPeriod, &clockParams);
    buttonClockHandle = Clock_handle(&buttonClockStruct);

    Task_Params_init(&sensorTaskParams);
    sensorTaskParams.stackSize = STACKSIZE;
    sensorTaskParams.stack = &sensorTaskStack;
    sensorTaskParams.priority = 2;
    Task_create(sensorTaskFxn, &sensorTaskParams, NULL);

    Task_Params_init(&uartTaskParams);
    uartTaskParams.stackSize = STACKSIZE;
    uartTaskParams.stack = &uartTaskStack;
    uartTaskParams.priority = 2;
    Task_create(uartTaskFxn, &uartTaskParams, NULL);

    BIOS_start();
    return 0;
}
