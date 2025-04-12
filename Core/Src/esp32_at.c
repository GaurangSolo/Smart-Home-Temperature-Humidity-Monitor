#include "esp32_at.h"
#include "config.h"   // Include your configuration
#include <stdio.h>    // For sprintf, printf
#include <string.h>   // For strlen, strstr, memset
#include <stdlib.h>   // Potentially for other functions

// External declaration of the UART handle used for ESP32 communication
extern UART_HandleTypeDef ESP32_UART_HANDLE;
// External declaration for debug UART if printf is used within these functions
extern UART_HandleTypeDef DEBUG_UART_HANDLE;

// --- Receive Buffer and Flags for Interrupt Handling ---
#define RX_BUFFER_SIZE 256
uint8_t esp32_rx_buffer[RX_BUFFER_SIZE];
uint8_t rx_byte; // Single byte buffer for HAL_UART_Receive_IT
volatile uint16_t esp32_rx_idx = 0;
volatile uint8_t rx_line_ready = 0; // Flag set by ISR when a line ('\n') is received

/**
  * @brief  Rx Transfer completed callback.
  * @note   This function MUST be defined somewhere in your project (e.g., stm32f1xx_it.c or main.c)
  * and will be called by the HAL UART IRQ Handler.
  * @param  huart UART handle.
  * @retval None
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  // Check if the interrupt is from the ESP32 UART
  if (huart->Instance == ESP32_UART_HANDLE.Instance)
  {
    // Store the received byte if buffer is not full
    if (esp32_rx_idx < RX_BUFFER_SIZE - 1) {
      esp32_rx_buffer[esp32_rx_idx++] = rx_byte;
      esp32_rx_buffer[esp32_rx_idx] = '\0'; // Null-terminate for string functions
    } else {
      // Buffer overflow handling (e.g., reset index or set error flag)
      // For simplicity, just reset index here. Consider a dedicated error flag.
      printf("WARN: ESP32 Rx Buffer Overflow!\r\n");
      esp32_rx_idx = 0;
      memset(esp32_rx_buffer, 0, RX_BUFFER_SIZE);
    }

    // Check if the received byte is a newline character, indicating end of a line/response segment
    // More sophisticated parsing could look for "OK", "ERROR", ">", etc. here,
    // but keeping the ISR simple is generally better. Let SendCommand parse the buffer.
    if (rx_byte == '\n') {
      rx_line_ready = 1; // Signal that at least one line has been received
      // Don't reset index here, SendCommand will check the buffer content
    }

    // Re-enable the interrupt to receive the next byte
    HAL_UART_Receive_IT(&ESP32_UART_HANDLE, &rx_byte, 1);
  }
}

/**
 * @brief Sends an AT command and waits for a specific response using UART Interrupts.
 * @note This version uses interrupts and is non-blocking during the wait, but parsing is still basic.
 * Needs robust parsing of the complete response buffer for production code.
 * @param cmd The AT command string to send (MUST end with \r\n).
 * @param expected_response The string expected within the received lines (e.g., "OK", ">"). NULL to skip check.
 * @param timeout_ms Timeout duration in milliseconds.
 * @retval HAL_OK on success/expected response, HAL_TIMEOUT if timeout, HAL_ERROR on Tx/Rx error or "ERROR" response.
 */
HAL_StatusTypeDef ESP32_SendCommand(const char* cmd, const char* expected_response, uint32_t timeout_ms) {
    HAL_StatusTypeDef tx_status;
    uint32_t start_time = HAL_GetTick();

    // --- Prepare for Reception ---
    memset(esp32_rx_buffer, 0, RX_BUFFER_SIZE); // Clear buffer
    esp32_rx_idx = 0;                           // Reset index
    rx_line_ready = 0;                          // Clear flag
    // Start listening for the first byte via interrupt
    if (HAL_UART_Receive_IT(&ESP32_UART_HANDLE, &rx_byte, 1) != HAL_OK) {
         printf("ESP UART Rx IT Start Error\r\n");
         return HAL_ERROR;
    }
    // ---------------------------

    printf("ESP CMD: %s", cmd); // Debug print

    // Send command
    tx_status = HAL_UART_Transmit(&ESP32_UART_HANDLE, (uint8_t*)cmd, strlen(cmd), UART_TIMEOUT);
    if (tx_status != HAL_OK) {
        printf("ESP UART Tx Error: %d\r\n", tx_status);
        HAL_UART_AbortReceive_IT(&ESP32_UART_HANDLE); // Abort IT receive on Tx error
        return tx_status;
    }

    // --- Wait for response line(s) or timeout ---
    while ((HAL_GetTick() - start_time) < timeout_ms) {
        if (rx_line_ready) { // Check flag set by ISR
            rx_line_ready = 0; // Consume the flag for this check cycle

            // Check buffer content for expected response or error
            // This parsing is still basic - only checks if substring exists
            if (expected_response != NULL && strstr((char*)esp32_rx_buffer, expected_response) != NULL) {
                printf("ESP RSP: Found '%s'\r\n", expected_response);
                // Optional: Clear buffer or reset index now if response fully processed
                // esp32_rx_idx = 0; memset(esp32_rx_buffer, 0, RX_BUFFER_SIZE);
                HAL_Delay(50); // Short delay for ESP processing before next command
                return HAL_OK; // Success!
            }
            if (strstr((char*)esp32_rx_buffer, "ERROR") != NULL) {
                printf("ESP RSP: ERROR detected\r\n");
                printf("Buffer: %s\r\n", esp32_rx_buffer);
                // Optional: Clear buffer or reset index
                return HAL_ERROR; // Error response received
            }
            // If we received a line but it wasn't what we expected (and not ERROR),
            // we might continue waiting for more lines up to the timeout.
            // More complex parsing would be needed here for multi-line responses.
        }
        // Yield CPU briefly to allow other tasks/interrupts
        HAL_Delay(1); // Small delay is crucial to prevent hogging CPU
    }

    // If loop finishes, timeout occurred
    HAL_UART_AbortReceive_IT(&ESP32_UART_HANDLE); // Abort IT receive on timeout
    printf("ESP RSP: Timeout (%lu ms) waiting for '%s'\r\n", (unsigned long)timeout_ms, expected_response ? expected_response : "any response");
    printf("Buffer content: %s\r\n", esp32_rx_buffer); // Print whatever was received
    return HAL_TIMEOUT;
}


// ============================================================================
// The rest of the functions (ESP32_InitWiFi, ESP32_MQTT_Config, etc.)
// remain structurally the same as before, as they just call ESP32_SendCommand.
// Make sure to verify the AT command strings within them!
// ============================================================================

/**
  * @brief Initializes WiFi Connection
  * @param ssid WiFi Network SSID
  * @param password WiFi Network Password
  * @retval HAL status
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
  * @brief Configures MQTT User Credentials for Adafruit IO
  * @param client_id Unique client ID string
  * @param username Adafruit IO Username string
  * @param key Adafruit IO Key string
  * @retval HAL status
  */
HAL_StatusTypeDef ESP32_MQTT_Config(const char* client_id, const char* username, const char* key) {
    char cmd_buffer[256];
    HAL_StatusTypeDef status;

    printf("Configuring MQTT...\r\n");
    // !! VERIFY SYNTAX with your specific ESP32 AT Firmware version !!
    sprintf(cmd_buffer, "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"\r\n",
            client_id, username, key);
    status = ESP32_SendCommand(cmd_buffer, "OK", 5000);
    if (status != HAL_OK) { printf("AT+MQTTUSERCFG failed\r\n"); return status; }

    printf("MQTT Configured.\r\n");
    return HAL_OK;
}

/**
  * @brief Connects to the MQTT Broker
  * @param host Broker hostname string
  * @param port Broker port number
  * @retval HAL status
  */
HAL_StatusTypeDef ESP32_MQTT_Connect(const char* host, uint16_t port) {
    char cmd_buffer[128];
    HAL_StatusTypeDef status;

    printf("Connecting MQTT Broker: %s:%d\r\n", host, port);
    // !! VERIFY SYNTAX with your specific ESP32 AT Firmware version !!
    sprintf(cmd_buffer, "AT+MQTTCONN=0,\"%s\",%d,0\r\n", host, port);
    status = ESP32_SendCommand(cmd_buffer, "OK", MQTT_CONNECT_TIMEOUT);
    if (status != HAL_OK) { printf("AT+MQTTCONN failed\r\n"); return status; }

    printf("MQTT Broker Connected.\r\n");
    return HAL_OK;
}

/**
  * @brief Publishes data to an MQTT topic using MQTTPUBRAW
  * @param topic MQTT topic string
  * @param data Data string to publish
  * @retval HAL status
  */
HAL_StatusTypeDef ESP32_MQTT_Publish(const char* topic, const char* data) {
    char cmd_buffer[256];
    HAL_StatusTypeDef status;
    int data_len = strlen(data);

    if (data_len <= 0) {
        printf("MQTT Publish Error: Empty data\r\n");
        return HAL_ERROR;
    }
    // Adafruit IO has limits on topic length and payload size, check them.
    if (strlen(topic) > 128 || data_len > 256) { // Example limits, check AIO docs
         printf("MQTT Publish Error: Topic or data too long\r\n");
         return HAL_ERROR;
    }


    printf("Publishing MQTT: Topic=[%s], Data=[%s]\n", topic, data);

    // Publish Data using MQTTPUBRAW
    // !! VERIFY SYNTAX with your specific ESP32 AT Firmware version !!

    // 1. Send the command indicating data length, wait for ">" prompt
    sprintf(cmd_buffer, "AT+MQTTPUBRAW=0,\"%s\",%d,0,0\r\n", topic, data_len);
    status = ESP32_SendCommand(cmd_buffer, ">", 5000); // Wait for prompt
    if (status != HAL_OK) {
        printf("MQTTPUBRAW command failed or no prompt '>'.\r\n");
        return status; // Return specific error? HAL_ERROR or status?
    }

    // 2. Send the actual data payload, wait for final "OK"
    // Need small delay after prompt sometimes
    HAL_Delay(100);
    // WARNING: SendCommand currently sends cmd+data. We need to send only data here.
    // Let's reuse SendCommand for simplicity, but ideally, a dedicated SendRawData function is better.
    // We send the data string and expect "OK" as the *final* confirmation.
    status = ESP32_SendCommand(data, "OK", MQTT_PUB_TIMEOUT); // Send data string, wait for final OK
    if (status != HAL_OK) {
        printf("MQTTPUBRAW data send failed or no OK received.\r\n");
        // Check Adafruit IO to see if data arrived anyway!
        return status;
    }

    printf("MQTT Published.\r\n");
    return HAL_OK;
}

/**
  * @brief Disconnects from the MQTT Broker
  * @param None
  * @retval HAL status
  */
HAL_StatusTypeDef ESP32_MQTT_Disconnect(void) {
    HAL_StatusTypeDef status;
    printf("Disconnecting MQTT...\r\n");
    // !! VERIFY SYNTAX with your specific ESP32 AT Firmware version !!
    status = ESP32_SendCommand("AT+MQTTDISCONN=0\r\n", "OK", 5000);
    printf("MQTT Disconnect command sent.\r\n");
    return status;
}

