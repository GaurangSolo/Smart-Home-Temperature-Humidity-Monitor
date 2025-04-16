#include "app_main.h"
#include "sht31.h"
#include "esp32_at.h"
#include "config.h"
#include <stdio.h>

// External peripheral handles declared in main.c / main.h
extern I2C_HandleTypeDef SHT31_I2C_HANDLE;
// extern UART_HandleTypeDef ESP32_UART_HANDLE; // Referenced via config.h -> esp32_at.c

/**
  * @brief Main application logic.
  */
void Application_Main(void) {

  float temperature, humidity;
  char temp_str[10];
  char humid_str[10];
  char temp_topic[128];
  char humid_topic[128];
  HAL_StatusTypeDef status;
  uint8_t wifi_ok = 0;
  uint8_t mqtt_ok = 0;

  // Construct MQTT topics (using defines from config.h)
  sprintf(temp_topic, "%s/feeds/%s", AIO_USERNAME, AIO_TEMP_FEED_NAME);
  sprintf(humid_topic, "%s/feeds/%s", AIO_USERNAME, AIO_HUMID_FEED_NAME);

  printf("\r\n--- STM32 Smart Home Monitor Starting ---\r\n");

  // --- Initialize WiFi ---
  printf("Attempting WiFi Connection...\r\n");
  status = ESP32_InitWiFi(WIFI_SSID, WIFI_PASSWORD);
  if (status == HAL_OK) {
      wifi_ok = 1;
      printf("WiFi Initialized.\r\n");
  } else {
      printf("!!! WiFi Init Failed! Check Credentials/Connections. !!!\r\n");
      // Consider adding robust retry or error indication here
  }

  // --- Configure MQTT ---
  if (wifi_ok) {
      printf("Configuring MQTT...\r\n");
      status = ESP32_MQTT_Config(MQTT_CLIENT_ID, AIO_USERNAME, AIO_KEY);
      if (status == HAL_OK) {
          mqtt_ok = 1;
          printf("MQTT Configured.\r\n");
      } else {
          printf("!!! MQTT Config Failed! Check AIO Credentials/AT Syntax. !!!\r\n");
      }
  }

  // --- Main Loop ---
  while (1) {
    if (!wifi_ok || !mqtt_ok) {
        printf("System Init Failed. Delaying...\r\n");
        HAL_Delay(10000); // Wait before potentially retrying init logic
        // Consider adding re-initialization logic here
        continue;
    }

    printf("\r\n-- Reading Sensor --\r\n");
    status = SHT31_Read(&SHT31_I2C_HANDLE, &temperature, &humidity);

    if (status == HAL_OK) {
      printf("Read OK: Temp=%.2f C, Humid=%.1f %%RH\r\n", temperature, humidity);

      // Format data for MQTT
      sprintf(temp_str, "%.2f", temperature);
      sprintf(humid_str, "%.1f", humidity);

      printf("-- Connecting MQTT Broker --\r\n");
      status = ESP32_MQTT_Connect("io.adafruit.com", 1883);

      if (status == HAL_OK) {
        printf("-- Publishing Data --\r\n");

        status = ESP32_MQTT_Publish(temp_topic, temp_str);
        if (status != HAL_OK) {
            printf("MQTT Publish Temp Failed! Status: %d\r\n", status);
        } else {
             printf("Temp published.\r\n");
        }
        HAL_Delay(500); // Small delay between publishes

        status = ESP32_MQTT_Publish(humid_topic, humid_str);
         if (status != HAL_OK) {
            printf("MQTT Publish Humid Failed! Status: %d\r\n", status);
         } else {
             printf("Humid published.\r\n");
         }
        HAL_Delay(500);

        ESP32_MQTT_Disconnect(); // Disconnect after publishing

      } else {
        printf("MQTT Connection Failed! Status: %d\r\n", status);
        // Consider trying to reconnect WiFi if MQTT fails repeatedly
      }

    } else {
      printf("Failed to read SHT31 sensor! Status: %d\r\n", status);
      // Handle sensor read error
    }

    printf("-- Delaying for %lu ms --\r\n", (unsigned long)SENSOR_READ_INTERVAL);
    HAL_Delay(SENSOR_READ_INTERVAL); // Wait before next cycle

  } // end while(1)
} // end Application_Main
