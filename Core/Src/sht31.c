#include "sht31.h"

// SHT31 I2C Address (default is 0x44)
#define SHT31_I2C_ADDR      (0x44 << 1)
#define SHT31_TIMEOUT       100 // I2C timeout in ms
#define SHT31_MEAS_DELAY    20  // Delay ms after triggering measurement

/**
 * @brief Reads Temp/Hum from SHT31 via I2C.
 */
HAL_StatusTypeDef SHT31_Read(I2C_HandleTypeDef *hi2c, float *temp_c, float *humidity_rh) {

    uint8_t cmd_measure[2] = {0x24, 0x00}; // Single shot, high repeatability, CS disabled
    uint8_t read_buffer[6];
    HAL_StatusTypeDef status;

    // Initialize output values to error state
    *temp_c = -999.0f;
    *humidity_rh = -999.0f;

    // Send Measurement Command
    status = HAL_I2C_Master_Transmit(hi2c, SHT31_I2C_ADDR, cmd_measure, 2, SHT31_TIMEOUT);
    if (status != HAL_OK) {
        return status;
    }

    // Wait for Measurement Completion
    HAL_Delay(SHT31_MEAS_DELAY);

    // Read Sensor Data (6 bytes)
    status = HAL_I2C_Master_Receive(hi2c, SHT31_I2C_ADDR, read_buffer, 6, SHT31_TIMEOUT);
    if (status != HAL_OK) {
        return status;
    }

    // --- Optional: Add CRC Check Here ---

    // Convert Raw Data
    uint16_t raw_temp = (read_buffer[0] << 8) | read_buffer[1];
    uint16_t raw_humidity = (read_buffer[3] << 8) | read_buffer[4];

    *temp_c = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
    *humidity_rh = 100.0f * ((float)raw_humidity / 65535.0f);

    // Basic range check
    if (*humidity_rh < 0.0f) *humidity_rh = 0.0f;
    if (*humidity_rh > 100.0f) *humidity_rh = 100.0f;

    return HAL_OK; // Success
}
