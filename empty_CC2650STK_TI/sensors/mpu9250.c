#include <inttypes.h>
#include <math.h>
#include <xdc/runtime/System.h>
#include <ti/drivers/I2C.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>

#include "Board.h"
#include "mpu9250.h"

#define PI      3.14159265

// Register addresses
#define PWR_MGMT_1       0x6B
#define GYRO_CONFIG      0x1B
#define ACCEL_CONFIG     0x1C
#define ACCEL_XOUT_H     0x3B
#define GYRO_XOUT_H      0x43
#define INT_PIN_CFG      0x37
#define INT_ENABLE       0x38
#define SMPLRT_DIV       0x19
#define CONFIG           0x1A

// Sensitivity scales
enum Ascale { AFS_2G = 0, AFS_4G, AFS_8G, AFS_16G };
enum Gscale { GFS_250DPS = 0, GFS_500DPS, GFS_1000DPS, GFS_2000DPS };

// Prototypes
void mpu9250_setup(I2C_Handle *i2c);
void initMPU9250();
void getGres();
void getAres();
void writeByte(uint8_t reg, uint8_t data);
void readByte(uint8_t reg, uint8_t count, uint8_t *data);
void delay(uint16_t delay);
void accelgyrocalMPU9250(float *dest1, float *dest2);

// Global variables
I2C_Handle i2c;
uint8_t Gscale = GFS_250DPS;
uint8_t Ascale = AFS_8G;
float aRes, gRes;
float gyroBias[3] = {0, 0, 0}, accelBias[3] = {0, 0, 0};

// Write a byte to a register
void writeByte(uint8_t reg, uint8_t data) {
    I2C_Transaction i2cTransaction;
    uint8_t txBuffer[2];
    txBuffer[0] = reg;
    txBuffer[1] = data;
    i2cTransaction.slaveAddress = Board_MPU9250_ADDR;
    i2cTransaction.writeBuf = txBuffer;
    i2cTransaction.writeCount = 2;
    i2cTransaction.readBuf = NULL;
    i2cTransaction.readCount = 0;

    if (!I2C_transfer(i2c, &i2cTransaction)) {
        System_printf("MPU9250: write failed\n");
    }
    System_flush();
}

// Read a byte from a register
void readByte(uint8_t reg, uint8_t count, uint8_t *data) {
    I2C_Transaction i2cTransaction;
    uint8_t txBuffer[1];
    txBuffer[0] = reg;
    i2cTransaction.slaveAddress = Board_MPU9250_ADDR;
    i2cTransaction.writeBuf = txBuffer;
    i2cTransaction.writeCount = 1;
    i2cTransaction.readBuf = data;
    i2cTransaction.readCount = count;

    if (!I2C_transfer(i2c, &i2cTransaction)) {
        System_printf("MPU9250: read failed\n");
    }
    System_flush();
}

// Delay function
void delay(uint16_t delay) {
    Task_sleep(delay * 1000 / Clock_tickPeriod);
}

// Calculate gyroscope resolution
void getGres() {
    switch (Gscale) {
        case GFS_250DPS: gRes = 250.0 / 32768.0; break;
        case GFS_500DPS: gRes = 500.0 / 32768.0; break;
        case GFS_1000DPS: gRes = 1000.0 / 32768.0; break;
        case GFS_2000DPS: gRes = 2000.0 / 32768.0; break;
    }
}

// Calculate accelerometer resolution
void getAres() {
    switch (Ascale) {
        case AFS_2G: aRes = 2.0 / 32768.0; break;
        case AFS_4G: aRes = 4.0 / 32768.0; break;
        case AFS_8G: aRes = 8.0 / 32768.0; break;
        case AFS_16G: aRes = 16.0 / 32768.0; break;
    }
}

// Setup the MPU9250
void mpu9250_setup(I2C_Handle *i2c_orig) {
    i2c = *i2c_orig;  // Dereference to get the actual I2C handle
    System_printf("MPU9250: Setup start...\n");
    System_flush();

    // Initialize the sensor
    initMPU9250();
    delay(100);

    // Get sensor resolutions
    getGres();
    getAres();

    System_printf("MPU9250: Setup complete\n");
    System_flush();
}

// Initialize the MPU9250 sensor
void initMPU9250() {
    // Wake up the device
    writeByte(PWR_MGMT_1, 0x00);
    delay(100);

    // Set up Gyroscope and Accelerometer
    writeByte(GYRO_CONFIG, 0x00);  // Set gyroscope full scale range
    writeByte(ACCEL_CONFIG, 0x00); // Set accelerometer full scale range
    writeByte(SMPLRT_DIV, 0x04);   // Set sample rate to 200 Hz
    writeByte(CONFIG, 0x03);       // Set low-pass filter

    // Configure interrupt settings
    writeByte(INT_PIN_CFG, 0x12);  // Interrupt setup
    writeByte(INT_ENABLE, 0x01);   // Enable data ready interrupt

    delay(100);
}

// Calibrate the sensors (accelerometer and gyroscope)
void accelgyrocalMPU9250(float *dest1, float *dest2) {
    uint8_t data[12];
    uint16_t ii, packet_count, fifo_count;
    int32_t gyro_bias[3] = {0, 0, 0}, accel_bias[3] = {0, 0, 0};

    // Reset device and initialize for calibration
    writeByte(PWR_MGMT_1, 0x80);
    delay(100);

    // Collect accelerometer and gyro data for calibration
    // TODO: Add data collection and bias calculation code

    dest1[0] = (float) gyro_bias[0] / 131.0;
    dest1[1] = (float) gyro_bias[1] / 131.0;
    dest1[2] = (float) gyro_bias[2] / 131.0;
    dest2[0] = (float) accel_bias[0] / 16384.0;
    dest2[1] = (float) accel_bias[1] / 16384.0;
    dest2[2] = (float) accel_bias[2] / 16384.0;
}
