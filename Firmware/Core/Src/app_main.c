#include "app_main.h"
#include "sht31.h"     // Include sensor header
#include "esp32_at.h"  // Include ESP32 AT command header
#include "config.h"    // Include your configuration defines
#include <stdio.h>     // For printf and sprintf

// External peripheral handles declared in main.c
extern I2C_HandleTypeDef SHT31_I2C_HANDLE;
// Declare UART handles if needed directly here, though esp32_at.c uses the extern from config.h
// extern UART_HandleTypeDef ESP32_UART_HANDLE;
// extern UART_HandleTypeDef DEBUG_UART_HANDLE;


/**
  * @brief Main application entry point. Called from main() after HAL init.
  * @param None
  * @retval None
  */
void Application_Main(void) {

  float temperature, humidity;
  char temp_str[10];
  char humid_str[10];
  char temp_topic[128]; // Buffer for MQTT topic string
  char humid_topic[128]; // Buffer for MQTT topic string
  HAL_StatusTypeDef status;
  uint8_t wifi_initialized = 0;
  uint8_t mqtt_configured = 0;

  // Construct MQTT topics once
  sprintf(temp_topic, "%s/feeds/%s", AIO_USERNAME, AIO_TEMP_FEED_NAME);
  sprintf(humid_topic, "%s/feeds/%s", AIO_USERNAME, AIO_HUMID_FEED_NAME);

  printf("\r\n--- STM32 Smart Home Monitor Starting ---\r\n");
  printf("Attempting to connect to WiFi: %s\r\n", WIFI_SSID);

  // --- Initialize WiFi ---
  // Add retry logic here for robustness
  status = ESP32_InitWiFi(WIFI_SSID, WIFI_PASSWORD);
  if (status == HAL_OK) {
      wifi_initialized = 1;
      printf("WiFi Initialized Successfully.\r\n");
  } else {
      printf("!!! Failed to connect to WiFi! Check SSID/Password. Halting. !!!\r\n");
      // Handle error - maybe blink LED, reset ESP32, or just stop here
      Error_Handler(); // Placeholder for critical error handling
  }

  // --- Configure MQTT ---
  // This only needs to be done once after power-up/reset
  if (wifi_initialized) {
      status = ESP32_MQTT_Config(MQTT_CLIENT_ID, AIO_USERNAME, AIO_KEY);
      if (status == HAL_OK) {
          mqtt_configured = 1;
          printf("MQTT Configured Successfully.\r\n");
      } else {
          printf("!!! Failed to configure MQTT! Check AIO Credentials/AT Syntax. Halting. !!!\r\n");
          Error_Handler();
      }
  }


  // --- Main Loop ---
  while (1) {
    if (!wifi_initialized || !mqtt_configured) {
        printf("System not initialized correctly. Halting loop.\r\n");
        HAL_Delay(5000); // Prevent spamming logs if init failed
        continue; // Skip the rest of the loop
    }

    printf("\r\n-- Reading Sensor --\r\n");
    status = SHT31_Read(&SHT31_I2C_HANDLE, &temperature, &humidity);

    if (status == HAL_OK) {
      printf("Read OK: Temp=%.2f C, Humid=%.1f %%RH\r\n", temperature, humidity);

      // Format data for MQTT
      sprintf(temp_str, "%.2f", temperature);
      sprintf(humid_str, "%.1f", humidity);

      printf("-- Connecting MQTT Broker --\r\n");
      status = ESP32_MQTT_Connect("io.adafruit.com", 1883); // Use port 1883 for standard MQTT

      if (status == HAL_OK) {
        printf("-- Publishing Data --\r\n");

        // Publish Temperature
        status = ESP32_MQTT_Publish(temp_topic, temp_str);
        if (status != HAL_OK) {
            printf("MQTT Publish Temp Failed! Status: %d\r\n", status);
            // Consider error handling - maybe retry?
        } else {
             printf("Temp published.\r\n");
        }
        HAL_Delay(500); // Small delay between publishes

        // Publish Humidity
        status = ESP32_MQTT_Publish(humid_topic, humid_str);
         if (status != HAL_OK) {
            printf("MQTT Publish Humid Failed! Status: %d\r\n", status);
            // Consider error handling
         } else {
             printf("Humid published.\r\n");
         }
        HAL_Delay(500);

        // Disconnect MQTT (optional, could keep connection open)
        ESP32_MQTT_Disconnect();

      } else {
        printf("MQTT Connection Failed! Status: %d\r\n", status);
        // Handle connection error (e.g., check WiFi, retry later)
      }

    } else {
      printf("Failed to read SHT31 sensor! Status: %d\r\n", status);
      // Handle sensor read error (e.g., retry, indicate error state)
    }

    printf("-- Delaying for %lu ms --\r\n", (unsigned long)SENSOR_READ_INTERVAL);
    HAL_Delay(SENSOR_READ_INTERVAL); // Wait before next cycle

  } // end while(1)
} // end Application_Main
