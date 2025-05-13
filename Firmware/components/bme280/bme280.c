#include "bme280.h"
#include "driver/i2c.h"
#include "config.h"
#include "esp_log.h"

#define BME280_ADDR 0x76 // Check BME280 address in schematic

void bme280_init() {
    // I2C initialization (shared with NPM1100)
    // Configured in config.h
}

void bme280_read_data(bme280_data_t *data) {
    // Read raw data via I2C and convert to actual values
    // Example implementation using Bosch BME280 driver
}
