#include <stdio.h>
#include <string.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/UART.h>
#include "Board.h"
#include "sensors/opt3001.h"
#include "sensors/mpu9250.h"

/* Task */
#define STACKSIZE 2048
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];

// State machine states
enum state { WAITING=1, DATA_READY, DOT, DASH, SPACE };
enum state programState = WAITING;

// Global variables
double ambientLight = -1000.0;
float ax, ay, az, gx, gy, gz;

// Button and LED configuration
static PIN_Handle buttonHandle;
static PIN_State buttonState;
static PIN_Handle ledHandle;
static PIN_State ledState;
PIN_Config buttonConfig[] = {
    Board_BUTTON0 | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
    PIN_TERMINATE
};
PIN_Config ledConfig[] = {
    Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

void buttonFxn(PIN_Handle handle, PIN_Id pinId) {
    // Handle button press
    if (pinId == Board_BUTTON0) {
        // Example: Toggle LED
        PIN_setOutputValue(ledHandle, Board_LED0, !PIN_getOutputValue(Board_LED0));
    }
}

/* Task Functions */
Void uartTaskFxn(UArg arg0, UArg arg1) {
    UART_Handle uart;
    UART_Params uartParams;

    // Initialize UART
    UART_Params_init(&uartParams);
    uartParams.baudRate = 9600;
    uart = UART_open(Board_UART0, &uartParams);
    if (uart == NULL) {
        System_abort("Error opening the UART");
    }

    while (1) {
        if (programState == DATA_READY) {
            // Send sensor data over UART
            char buffer[128];
            snprintf(buffer, sizeof(buffer), "ax: %f, ay: %f, az: %f, gx: %f, gy: %f, gz: %f\n", ax, ay, az, gx, gy, gz);
            UART_write(uart, buffer, strlen(buffer));

            // Provide feedback to the user
            PIN_setOutputValue(ledHandle, Board_LED0, 1); // Turn on LED
            Task_sleep(500000 / Clock_tickPeriod); // 500 ms delay
            PIN_setOutputValue(ledHandle, Board_LED0, 0); // Turn off LED

            // Reset state
            programState = WAITING;
        }

        // Once per second, you can modify this
        Task_sleep(1000000 / Clock_tickPeriod);
    }
}

Void sensorTaskFxn(UArg arg0, UArg arg1) {
    I2C_Handle i2c;
    I2C_Params i2cParams;

    // Initialize I2C
    I2C_Params_init(&i2cParams);
    i2c = I2C_open(Board_I2C, &i2cParams);
    if (i2c == NULL) {
        System_abort("Error Initializing I2C\n");
    }

    // Initialize MPU9250
    mpu9250_setup(&i2c);

    while (1) {
        // Read sensor data
        mpu9250_get_data(&i2c, &ax, &ay, &az, &gx, &gy, &gz);

      // Process sensor data to recognize commands
        if (ax > 1.0) {
            programState = DOT; // Quick tilt to the right
        } else if (ax < -1.0) {
            programState = DASH; // Quick tilt to the left
        } else if (az > 1.0) {
            programState = SPACE; // Quick upward movement
        } else {
            programState = WAITING;
        }

        // Save the sensor value into the global variable
        ambientLight = ax; // Example: save ax value, modify as needed

        // Print sensor data to Debug window
        System_printf("ax: %f, ay: %f, az: %f, gx: %f, gy: %f, gz: %f\n", ax, ay, az, gx, gy, gz);
        System_flush();

        // Once per second, you can modify this
        Task_sleep(1000000 / Clock_tickPeriod);
    }
}

Int main(void) {
    // Task variables
    Task_Handle sensorTaskHandle;
    Task_Params sensorTaskParams;
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;

    // Initialize board
    Board_initGeneral();
    I2C_init();
    UART_init();

    // Initialize button and LED pins
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

    // Initialize tasks
    Task_Params_init(&sensorTaskParams);
    sensorTaskParams.stackSize = STACKSIZE;
    sensorTaskParams.stack = &sensorTaskStack;
    sensorTaskParams.priority = 2;
    sensorTaskHandle = Task_create(sensorTaskFxn, &sensorTaskParams, NULL);

    Task_Params_init(&uartTaskParams);
    uartTaskParams.stackSize = STACKSIZE;
    uartTaskParams.stack = &uartTaskStack;
    uartTaskParams.priority = 2;
    uartTaskHandle = Task_create(uartTaskFxn, &uartTaskParams, NULL);

    // Start BIOS
    BIOS_start();
    return 0;
}
