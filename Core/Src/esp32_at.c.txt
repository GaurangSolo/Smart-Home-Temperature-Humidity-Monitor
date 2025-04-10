#include "esp32_at.h"
#include "config.h"   // Include your configuration
#include <stdio.h>    // For sprintf, printf
#include <string.h>   // For strlen, strstr
#include <stdlib.h>   // Potentially for other functions

// External declaration of the UART handle used for ESP32 communication
// Ensure this matches the handle defined in main.c / config.h
extern UART_HandleTypeDef ESP32_UART_HANDLE;

// Buffer for receiving responses - adjust size as needed
// Consider making this non-global or using better buffer management in a real app
#define RX_BUFFER_SIZE 256
uint8_t esp32_rx_buffer[RX_BUFFER_SIZE];
volatile uint16_t esp32_rx_idx = 0;
// Add flags or mechanisms for non-blocking reception if using interrupts/DMA

/**
 * @brief Sends an AT command and waits for a specific response (Simplified Blocking Example).
 * @note THIS IS A VERY SIMPLIFIED EXAMPLE! Needs robust non-blocking reception,
 * response parsing, error handling, and handling of unsolicited messages.
 * @param cmd The AT command string to send (MUST end with \r\n).
 * @param expected_response The string expected in the response (e.g., "OK"). NULL to skip check.
 * @param timeout_ms Timeout duration in milliseconds.
 * @retval HAL_OK on success/expected response, HAL_TIMEOUT if timeout, HAL_ERROR on Tx error.
 */
HAL_StatusTypeDef ESP32_SendCommand(const char* cmd, const char* expected_response, uint32_t timeout_ms) {
    HAL_StatusTypeDef tx_status;
    uint32_t start_time = HAL_GetTick();

    // --- Clear Rx Buffer Logic (Example - needs proper implementation) ---
    // If using DMA/Interrupts, you'd likely reset DMA pointers or clear flags here.
    memset(esp32_rx_buffer, 0, RX_BUFFER_SIZE);
    esp32_rx_idx = 0;
    // Start listening if using IT/DMA: HAL_UART_Receive_DMA(&ESP32_UART_HANDLE, esp32_rx_buffer, RX_BUFFER_SIZE);
    // --------------------------------------------------------------------

    printf("ESP CMD: %s", cmd); // Debug print

    // Send command
    tx_status = HAL_UART_Transmit(&ESP32_UART_HANDLE, (uint8_t*)cmd, strlen(cmd), UART_TIMEOUT);
    if (tx_status != HAL_OK) {
        printf("ESP UART Tx Error: %d\r\n", tx_status);
        return tx_status;
    }

    // --- Simplified Blocking Wait for Response ---
    // WARNING: This is inefficient and basic. Replace with non-blocking methods.
    do {
        // Attempt to receive data (blocking example, short timeout)
        // In a real app, check a flag set by Rx interrupt/DMA completion callback
        HAL_StatusTypeDef rx_status = HAL_UART_Receive(&ESP32_UART_HANDLE, &esp32_rx_buffer[esp32_rx_idx], 1, 10); // Read 1 byte, 10ms timeout
        if (rx_status == HAL_OK) {
            if (esp32_rx_idx < RX_BUFFER_SIZE - 1) {
                esp32_rx_idx++;
            }
        } else if (rx_status != HAL_TIMEOUT) {
             printf("ESP UART Rx Error: %d\r\n", rx_status);
             // return rx_status; // Optionally return error immediately
        }

        // Check if expected response is found in the buffer
        if (expected_response != NULL && strstr((char*)esp32_rx_buffer, expected_response) != NULL) {
            printf("ESP RSP: Found '%s'\r\n", expected_response);
            // Add short delay for module processing if needed before next command
            HAL_Delay(100);
            return HAL_OK;
        }

        // Check for "ERROR" response
        if (strstr((char*)esp32_rx_buffer, "ERROR") != NULL) {
             printf("ESP RSP: ERROR detected\r\n");
             return HAL_ERROR; // Indicate error detected
        }

        // Prevent infinite loop if buffer fills without expected response
        if (esp32_rx_idx >= RX_BUFFER_SIZE -1) {
            printf("ESP RSP: Rx buffer full, no expected response found.\r\n");
            return HAL_ERROR;
        }

    } while ((HAL_GetTick() - start_time) < timeout_ms);

    // If loop finishes, timeout occurred
    printf("ESP RSP: Timeout (%lu ms)\r\n", (unsigned long)timeout_ms);
    return HAL_TIMEOUT;
}


HAL_StatusTypeDef ESP32_InitWiFi(const char* ssid, const char* password) {
    char cmd_buffer[128];
    HAL_StatusTypeDef status;

    printf("Initializing WiFi...\r\n");
    // Optional: Reset ESP32 module first? (Requires hardware connection) AT+RST
    // ESP32_SendCommand("AT+RST\r\n", "ready", 5000); HAL_Delay(2000);

    status = ESP32_SendCommand("ATE0\r\n", "OK", UART_TIMEOUT); // Turn off echo
    if (status != HAL_OK) printf("ATE0 failed (continuing...)\r\n"); // Non-critical usually

    status = ESP32_SendCommand("AT+CWMODE=1\r\n", "OK", 2000); // Set Station mode
    if (status != HAL_OK) return status;

    // Check current connection status first? AT+CWJAP?
    // status = ESP32_SendCommand("AT+CWJAP?\r\n", WIFI_SSID, 5000); // Check if already connected
    // if (status == HAL_OK) { printf("Already connected to WiFi.\r\n"); return HAL_OK; }

    // Connect to Access Point
    sprintf(cmd_buffer, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, password);
    status = ESP32_SendCommand(cmd_buffer, "OK", WIFI_CONNECT_TIMEOUT); // Long timeout for connection
    if (status != HAL_OK) return status;

    printf("WiFi Connected.\r\n");
    return HAL_OK;
}

HAL_StatusTypeDef ESP32_MQTT_Config(const char* client_id, const char* username, const char* key) {
    char cmd_buffer[256]; // Increased size for potentially long keys/usernames
    HAL_StatusTypeDef status;

    printf("Configuring MQTT...\r\n");
    // Configure MQTT User Properties (Broker URL/Port set in MQTTCONN)
    // Format: AT+MQTTUSERCFG=<LinkID>,<Scheme>,<"ClientID">,<"Username">,<"Password">,<CertKeyID>,<CACertID>,<"Path">
    // Scheme 1 = TCP MQTT
    // NOTE: Verify this syntax with your ESP32 AT Firmware documentation!
    sprintf(cmd_buffer, "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"\r\n",
            client_id, username, key);
    status = ESP32_SendCommand(cmd_buffer, "OK", 5000);
    if (status != HAL_OK) return status;

    // Optional: Configure MQTT Connection parameters (e.g., keepalive)
    // AT+MQTTCONNCFG=<LinkID>,<Keepalive>,<DisableCleanSession>,<"LWT_Topic">,<"LWT_Msg">,<LWT_QoS>,<LWT_Retain>
    // Example: Set keepalive to 60 seconds
    // status = ESP32_SendCommand("AT+MQTTCONNCFG=0,60,0,\"\",\"\",0,0\r\n", "OK", 5000);
    // if (status != HAL_OK) return status;

    printf("MQTT Configured.\r\n");
    return HAL_OK;
}

HAL_StatusTypeDef ESP32_MQTT_Connect(const char* host, uint16_t port) {
    char cmd_buffer[128];
    HAL_StatusTypeDef status;

    printf("Connecting MQTT Broker...\r\n");
    // Connect to MQTT Broker
    // Format: AT+MQTTCONN=<LinkID>,<"Host">,<Port>,<Reconnect>
    // Reconnect=1 enables auto-reconnect (check your firmware version)
    // NOTE: Verify this syntax with your ESP32 AT Firmware documentation!
    sprintf(cmd_buffer, "AT+MQTTCONN=0,\"%s\",%d,0\r\n", host, port); // Reconnect=0 for manual control
    status = ESP32_SendCommand(cmd_buffer, "OK", MQTT_CONNECT_TIMEOUT);
    if (status != HAL_OK) return status;

    printf("MQTT Connected.\r\n");
    return HAL_OK;
}

HAL_StatusTypeDef ESP32_MQTT_Publish(const char* topic, const char* data) {
    char cmd_buffer[256]; // Adjust size based on max topic/data length
    HAL_StatusTypeDef status;
    int data_len = strlen(data);

    printf("Publishing MQTT: Topic=%s, Data=%s\n", topic, data);

    // Publish Data using MQTTPUBRAW (often more reliable)
    // Format: AT+MQTTPUBRAW=<LinkID>,<"Topic">,<Length>,<QoS>,<Retain>
    // QoS=0, Retain=0 are common defaults
    // NOTE: Verify this syntax with your ESP32 AT Firmware documentation!

    // 1. Send the command indicating data length, wait for ">" prompt
    sprintf(cmd_buffer, "AT+MQTTPUBRAW=0,\"%s\",%d,0,0\r\n", topic, data_len);
    status = ESP32_SendCommand(cmd_buffer, ">", 5000); // Wait for prompt
    if (status != HAL_OK) {
        printf("MQTTPUBRAW command failed or no prompt received.\r\n");
        return status;
    }

    // 2. Send the actual data payload followed by \r\n (?) - Check AT docs.
    // Often, just sending data is enough, then wait for "OK".
    // Let's try sending data then waiting for OK.
    HAL_Delay(50); // Short delay before sending data after prompt
    status = ESP32_SendCommand(data, "OK", MQTT_PUB_TIMEOUT); // Send data, wait for final OK
    if (status != HAL_OK) {
        printf("MQTTPUBRAW data send failed.\r\n");
        return status;
    }

    printf("MQTT Published.\r\n");
    return HAL_OK;
}

HAL_StatusTypeDef ESP32_MQTT_Disconnect(void) {
    HAL_StatusTypeDef status;
    printf("Disconnecting MQTT...\r\n");
    // Format: AT+MQTTDISCONN=<LinkID>
    // NOTE: Verify this syntax with your ESP32 AT Firmware documentation!
    status = ESP32_SendCommand("AT+MQTTDISCONN=0\r\n", "OK", 5000);
    // Ignore status slightly, as we might disconnect anyway
    printf("MQTT Disconnected command sent.\r\n");
    return status;
}

