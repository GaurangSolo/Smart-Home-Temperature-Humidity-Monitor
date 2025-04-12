#ifndef INC_ESP32_AT_H_
#define INC_ESP32_AT_H_

#include "main.h" // Includes HAL drivers
#include <stdint.h> // For uintxx_t types

// Function Prototypes
HAL_StatusTypeDef ESP32_SendCommand(const char* cmd, const char* expected_response, uint32_t timeout_ms);
HAL_StatusTypeDef ESP32_InitWiFi(const char* ssid, const char* password);
HAL_StatusTypeDef ESP32_MQTT_Config(const char* client_id, const char* username, const char* key);
HAL_StatusTypeDef ESP32_MQTT_Connect(const char* host, uint16_t port);
HAL_StatusTypeDef ESP32_MQTT_Publish(const char* topic, const char* data);
HAL_StatusTypeDef ESP32_MQTT_Disconnect(void);

// Add any other ESP32 related functions or definitions here

#endif /* INC_ESP32_AT_H_ */
