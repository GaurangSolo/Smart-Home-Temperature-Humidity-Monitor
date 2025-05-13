#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "bme280.h"
#include "npm1100.h"
#include "wifi_mqtt.h"
#include "config.h"

static const char *TAG = "Main";

void app_main() {
    // Initialize I2C
    i2c_init();

    // Read sensor data
    bme280_data_t sensor_data = {0};
    bme280_read_data(&sensor_data);

    // Read battery voltage
    float battery_voltage = npm1100_read_battery();

    // Read PMIC status
    npm1100_status_t pmic_status = npm1100_get_status();

    // Connect to WiFi and publish data
    wifi_connect();
    mqtt_publish_data(&sensor_data, battery_voltage, &pmic_status);

    // Enter deep sleep (e.g., 10 minutes)
    ESP_LOGI(TAG, "Entering deep sleep");
    esp_deep_sleep(600000000); // 600 seconds = 10 minutes
}
