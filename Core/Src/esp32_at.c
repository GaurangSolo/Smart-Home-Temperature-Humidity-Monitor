#include "esp32_at.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// External HAL Handles (defined in main.c)
extern UART_HandleTypeDef ESP32_UART_HANDLE;
extern UART_HandleTypeDef DEBUG_UART_HANDLE; // For printf

// --- Receive Buffer and Flags for Interrupt Handling ---
#define RX_BUFFER_SIZE 256
uint8_t esp32_rx_buffer[RX_BUFFER_SIZE];
uint8_t rx_byte; // Single byte buffer for HAL_UART_Receive_IT
volatile uint16_t esp32_rx_idx = 0;
volatile uint8_t rx_line_ready = 0; // Flag set by ISR when '\n' is received

/**
  * @brief  Rx Transfer completed callback - Called by HAL_UART_IRQHandler.
  * @note   Must be defined in project (e.g., stm32f1xx_it.c or main.c).
  * @param  huart UART handle.
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == ESP32_UART_HANDLE.Instance)
  {
    if (esp32_rx_idx < RX_BUFFER_SIZE - 1) {
      esp32_rx_buffer[esp32_rx_idx++] = rx_byte;
      esp32_rx_buffer[esp32_rx_idx] = '\0'; // Null-terminate
    } else {
      // Basic Overflow handling: Reset index
      printf("WARN: ESP32 Rx Buffer Overflow!\r\n");
      esp32_rx_idx = 0;
      memset(esp32_rx_buffer, 0, RX_BUFFER_SIZE);
    }

    // Signal that a line might be ready when newline is received.
    // SendCommand function will parse the buffer content.
    if (rx_byte == '\n') {
      rx_line_ready = 1;
    }

    // Re-enable interrupt for next byte
    HAL_UART_Receive_IT(&ESP32_UART_HANDLE, &rx_byte, 1);
  }
}

/**
 * @brief Sends AT command, waits for response using Interrupts (Simplified).
 * @warning Response parsing is basic (strstr for expected/ERROR). Needs improvement for robustness.
 * @warning Uses HAL_Delay in wait loop - better to use RTOS or non-blocking state machine.
 */
HAL_StatusTypeDef ESP32_SendCommand(const char* cmd, const char* expected_response, uint32_t timeout_ms) {
    HAL_StatusTypeDef tx_status;
    uint32_t start_time = HAL_GetTick();

    // Prepare for reception
    memset(esp32_rx_buffer, 0, RX_BUFFER_SIZE);
    esp32_rx_idx = 0;
    rx_line_ready = 0;
    if (HAL_UART_Receive_IT(&ESP32_UART_HANDLE, &rx_byte, 1) != HAL_OK) {
         printf("ESP UART Rx IT Start Error\r\n");
         return HAL_ERROR;
    }

    printf("ESP CMD: %s", cmd); // Debug print

    // Send command
    tx_status = HAL_UART_Transmit(&ESP32_UART_HANDLE, (uint8_t*)cmd, strlen(cmd), UART_TIMEOUT);
    if (tx_status != HAL_OK) {
        printf("ESP UART Tx Error: %d\r\n", tx_status);
        HAL_UART_AbortReceive_IT(&ESP32_UART_HANDLE);
        return tx_status;
    }

    // Wait for response line flag or timeout
    while ((HAL_GetTick() - start_time) < timeout_ms) {
        if (rx_line_ready) {
            rx_line_ready = 0; // Consume flag for this check

            // Check buffer content for expected response or error
            if (strstr((char*)esp32_rx_buffer, "ERROR") != NULL) {
                 printf("ESP RSP: ERROR detected\r\n");
                 // Optional: Clear buffer? HAL_UART_AbortReceive_IT(&ESP32_UART_HANDLE);
                 return HAL_ERROR;
            }
            if (expected_response != NULL && strstr((char*)esp32_rx_buffer, expected_response) != NULL) {
                printf("ESP RSP: Found '%s'\r\n", expected_response);
                HAL_Delay(50); // Short delay
                return HAL_OK;
            }
            // If line received but not expected/error, might need to wait longer for more lines
            // or implement better multi-line parsing. Resetting index to continue buffering.
            // esp32_rx_idx = 0;
            // memset(esp32_rx_buffer, 0, RX_BUFFER_SIZE);
            // HAL_UART_Receive_IT(&ESP32_UART_HANDLE, &rx_byte, 1); // Re-arm if needed after parsing a line
        }
        HAL_Delay(1); // Yield CPU
    }

    // Timeout occurred
    HAL_UART_AbortReceive_IT(&ESP32_UART_HANDLE);
    printf("ESP RSP: Timeout (%lu ms) waiting for '%s'\r\n", (unsigned long)timeout_ms, expected_response ? expected_response : "any response");
    printf("Buffer content: %s\r\n", esp32_rx_buffer);
    return HAL_TIMEOUT;
}


/**
  * @brief Initializes WiFi Connection.
  */
HAL_StatusTypeDef ESP32_InitWiFi(const char* ssid, const char* password) {
    char cmd_buffer[128];
    HAL_StatusTypeDef status;

    printf("Initializing WiFi...\r\n");

    status = ESP32_SendCommand("ATE0\r\n", "OK", UART_TIMEOUT);
    if (status != HAL_OK) printf("ATE0 failed (continuing...)\r\n");

    status = ESP32_SendCommand("AT+CWMODE=1\r\n", "OK", 2000);
    if (status != HAL_OK) { printf("AT+CWMODE failed\r\n"); return status; }

    printf("Connecting to SSID: %s\r\n", ssid);
    sprintf(cmd_buffer, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, password);
    status = ESP32_SendCommand(cmd_buffer, "OK", WIFI_CONNECT_TIMEOUT);
    if (status != HAL_OK) { printf("AT+CWJAP failed\r\n"); return status; }

    printf("WiFi Connected.\r\n");
    return HAL_OK;
}

/**
  * @brief Configures MQTT User Credentials for Adafruit IO.
  */
HAL_StatusTypeDef ESP32_MQTT_Config(const char* client_id, const char* username, const char* key) {
    char cmd_buffer[256];
    HAL_StatusTypeDef status;

    printf("Configuring MQTT...\r\n");
    // !! VERIFY AT SYNTAX !!
    sprintf(cmd_buffer, "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"\r\n",
            client_id, username, key);
    status = ESP32_SendCommand(cmd_buffer, "OK", 5000);
    if (status != HAL_OK) { printf("AT+MQTTUSERCFG failed\r\n"); return status; }

    printf("MQTT Configured.\r\n");
    return HAL_OK;
}

/**
  * @brief Connects to the MQTT Broker.
  */
HAL_StatusTypeDef ESP32_MQTT_Connect(const char* host, uint16_t port) {
    char cmd_buffer[128];
    HAL_StatusTypeDef status;

    printf("Connecting MQTT Broker: %s:%d\r\n", host, port);
    // !! VERIFY AT SYNTAX !!
    sprintf(cmd_buffer, "AT+MQTTCONN=0,\"%s\",%d,0\r\n", host, port);
    status = ESP32_SendCommand(cmd_buffer, "OK", MQTT_CONNECT_TIMEOUT);
    if (status != HAL_OK) { printf("AT+MQTTCONN failed\r\n"); return status; }

    printf("MQTT Broker Connected.\r\n");
    return HAL_OK;
}

/**
  * @brief Publishes data to an MQTT topic using MQTTPUBRAW.
  */
HAL_StatusTypeDef ESP32_MQTT_Publish(const char* topic, const char* data) {
    char cmd_buffer[256];
    HAL_StatusTypeDef status;
    int data_len = strlen(data);

    if (data_len <= 0) return HAL_ERROR;
    // Add length checks for topic/data based on Adafruit IO limits if needed

    printf("Publishing MQTT: Topic=[%s], Data=[%s]\n", topic, data);

    // 1. Send PUBRAW command, wait for ">" prompt
    // !! VERIFY AT SYNTAX !!
    sprintf(cmd_buffer, "AT+MQTTPUBRAW=0,\"%s\",%d,0,0\r\n", topic, data_len);
    status = ESP32_SendCommand(cmd_buffer, ">", 5000);
    if (status != HAL_OK) {
        printf("MQTTPUBRAW command failed or no prompt '>'.\r\n");
        return status;
    }

    // 2. Send data payload, wait for final "OK"
    HAL_Delay(100); // Small delay often helps
    // WARNING: Using SendCommand here assumes data doesn't contain "OK" or "ERROR"
    // A dedicated raw send function would be safer.
    status = ESP32_SendCommand(data, "OK", MQTT_PUB_TIMEOUT);
    if (status != HAL_OK) {
        printf("MQTTPUBRAW data send failed or no OK received.\r\n");
        return status;
    }

    printf("MQTT Published.\r\n");
    return HAL_OK;
}

/**
  * @brief Disconnects from the MQTT Broker.
  */
HAL_StatusTypeDef ESP32_MQTT_Disconnect(void) {
    HAL_StatusTypeDef status;
    printf("Disconnecting MQTT...\r\n");
    // !! VERIFY AT SYNTAX !!
    status = ESP32_SendCommand("AT+MQTTDISCONN=0\r\n", "OK", 5000);
    printf("MQTT Disconnect command sent.\r\n");
    return status;
}
