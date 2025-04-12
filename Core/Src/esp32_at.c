#include "esp32_at.h"
#include "config.h"   // Include your configuration (defines WIFI_SSID, AIO_USERNAME, etc. and peripheral handles)
#include <stdio.h>    // For sprintf, printf
#include <string.h>   // For strlen, strstr
#include <stdlib.h>   // Potentially for other functions

// External declaration of the UART handle used for ESP32 communication
// Ensure ESP32_UART_HANDLE is defined correctly in config.h or main.h/main.c
extern UART_HandleTypeDef ESP32_UART_HANDLE;
// External declaration for debug UART if printf is used within these functions
extern UART_HandleTypeDef DEBUG_UART_HANDLE;


// Buffer for receiving responses - adjust size as needed
// WARNING: Global buffer without proper clearing/indexing in SendCommand is risky.
// Better implementation would pass buffer pointers or use static within SendCommand with clearing.
#define RX_BUFFER_SIZE 256
uint8_t esp32_rx_buffer[RX_BUFFER_SIZE];
volatile uint16_t esp32_rx_idx = 0; // Simple index for the basic SendCommand example


// --- Helper Function for printf Debugging (If needed within this file) ---
// Duplicate from main.c or include a common debug header
#ifdef __GNUC__
  #define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
  #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif
/* // Uncomment if printf is needed directly from this file
PUTCHAR_PROTOTYPE
{
  HAL_UART_Transmit(&DEBUG_UART_HANDLE, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}
*/

/**
 * @brief Sends an AT command and waits for a specific response (Simplified Blocking Example).
 * @note THIS IS A VERY SIMPLIFIED EXAMPLE! Needs robust non-blocking reception (Interrupt/DMA),
 * proper buffer management, full response parsing (handling unsolicited messages like +IPD),
 * and better error recovery for real applications.
 * @param cmd The AT command string to send (MUST end with \r\n).
 * @param expected_response The string expected in the response (e.g., "OK", ">"). NULL to ignore response check.
 * @param timeout_ms Timeout duration in milliseconds.
 * @retval HAL_OK on success/expected response, HAL_TIMEOUT if timeout, HAL_ERROR on Tx/Rx error or "ERROR" response.
 */
HAL_StatusTypeDef ESP32_SendCommand(const char* cmd, const char* expected_response, uint32_t timeout_ms) {
    HAL_StatusTypeDef status;
    uint32_t start_time = HAL_GetTick();

    // --- Clear Rx Buffer (Basic) ---
    memset(esp32_rx_buffer, 0, RX_BUFFER_SIZE);
    esp32_rx_idx = 0;
    // Consider starting non-blocking receive here if using IT/DMA
    // HAL_UART_Receive_DMA(&ESP32_UART_HANDLE, esp32_rx_buffer, RX_BUFFER_SIZE);
    // --------------------------------

    printf("ESP CMD: %s", cmd); // Debug print

    // Send command
    status = HAL_UART_Transmit(&ESP32_UART_HANDLE, (uint8_t*)cmd, strlen(cmd), UART_TIMEOUT);
    if (status != HAL_OK) {
        printf("ESP UART Tx Error: %d\r\n", status);
        return status;
    }

    // --- Simplified Blocking Wait & Check ---
    // WARNING: Inefficient and not robust.
    do {
        // Basic blocking receive attempt (replace with non-blocking check)
        // This HAL_UART_Receive is just to simulate getting data into buffer for strstr check
        // A real implementation needs interrupt/DMA driven reception.
        uint8_t temp_buf[1]; // Read one byte at a time
        if (HAL_UART_Receive(&ESP32_UART_HANDLE, temp_buf, 1, 10) == HAL_OK) { // Short timeout poll
             if (esp32_rx_idx < RX_BUFFER_SIZE - 1) {
                 esp32_rx_buffer[esp32_rx_idx++] = temp_buf[0];
                 // Optional: Echo received char to debug UART? printf("%c", temp_buf[0]);
             } else {
                  printf("ESP RSP Buffer Full!\r\n");
                  return HAL_ERROR; // Buffer overflow
             }
        }

        // Check for expected response (if provided)
        if (expected_response != NULL && strstr((char*)esp32_rx_buffer, expected_response) != NULL) {
            printf("ESP RSP: Found '%s'\r\n", expected_response);
            HAL_Delay(100); // Short delay for ESP processing
            return HAL_OK;
        }

        // Check for "ERROR" response explicitly
        if (strstr((char*)esp32_rx_buffer, "ERROR") != NULL) {
             printf("ESP RSP: ERROR detected\r\n");
             return HAL_ERROR;
        }

    } while ((HAL_GetTick() - start_time) < timeout_ms);

    // If loop finishes, timeout occurred
    printf("ESP RSP: Timeout (%lu ms) waiting for '%s'\r\n", (unsigned long)timeout_ms, expected_response ? expected_response : "any response");
    printf("Buffer content: %s\r\n", esp32_rx_buffer); // Print what was received
    return HAL_TIMEOUT;
}


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

    // Optional: Reset module first? Requires wiring & delay
    // printf("Resetting ESP32...\r\n");
    // status = ESP32_SendCommand("AT+RST\r\n", "ready", 5000); // Wait for "ready" after reset
    // if(status != HAL_OK) return status;
    // HAL_Delay(2000); // Wait for module to stabilize

    status = ESP32_SendCommand("ATE0\r\n", "OK", UART_TIMEOUT); // Turn off echo
    if (status != HAL_OK) printf("ATE0 failed (continuing...)\r\n"); // Non-critical

    status = ESP32_SendCommand("AT+CWMODE=1\r\n", "OK", 2000); // Set Station mode
    if (status != HAL_OK) { printf("AT+CWMODE failed\r\n"); return status; }

    // Optional: Check if already connected?
    // status = ESP32_SendCommand("AT+CWJAP?\r\n", ssid, 5000);
    // if (status == HAL_OK) { printf("Already connected to WiFi.\r\n"); return HAL_OK; }
    // HAL_Delay(500); // Delay if checking

    // Connect to Access Point
    printf("Connecting to SSID: %s\r\n", ssid);
    sprintf(cmd_buffer, "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, password);
    status = ESP32_SendCommand(cmd_buffer, "OK", WIFI_CONNECT_TIMEOUT); // Long timeout for connection
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
    char cmd_buffer[256]; // Ensure buffer is large enough
    HAL_StatusTypeDef status;

    printf("Configuring MQTT...\r\n");
    // Configure MQTT User Properties
    // Format: AT+MQTTUSERCFG=<LinkID>,<Scheme>,<"ClientID">,<"Username">,<"Password">,<CertKeyID>,<CACertID>,<"Path">
    // Scheme 1 = TCP MQTT (no SSL/TLS)
    // !! VERIFY SYNTAX with your specific ESP32 AT Firmware version !!
    sprintf(cmd_buffer, "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"\r\n",
            client_id, username, key);
    status = ESP32_SendCommand(cmd_buffer, "OK", 5000);
    if (status != HAL_OK) { printf("AT+MQTTUSERCFG failed\r\n"); return status; }

    // Optional: Configure Keepalive (e.g., 120 seconds)
    // sprintf(cmd_buffer, "AT+MQTTCONNCFG=0,120,0,\"\",\"\",0,0\r\n");
    // status = ESP32_SendCommand(cmd_buffer, "OK", 5000);
    // if (status != HAL_OK) { printf("AT+MQTTCONNCFG failed\r\n"); return status; }

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
    // Connect to MQTT Broker
    // Format: AT+MQTTCONN=<LinkID>,<"Host">,<Port>,<Reconnect>
    // Reconnect=0 means disable auto-reconnect by module
    // !! VERIFY SYNTAX with your specific ESP32 AT Firmware version !!
    sprintf(cmd_buffer, "AT+MQTTCONN=0,\"%s\",%d,0\r\n", host, port);
    status = ESP32_SendCommand(cmd_buffer, "OK", MQTT_CONNECT_TIMEOUT); // Long timeout
    if (status != HAL_OK) { printf("AT+MQTTCONN failed\r\n"); return status; }

    printf("MQTT Broker Connected.\r\n");
    // Note: Module might send unsolicited "+MQTTCONNECTED" message too.
    // Robust parser should handle/ignore this.
    return HAL_OK;
}

/**
  * @brief Publishes data to an MQTT topic using MQTTPUBRAW
  * @param topic MQTT topic string
  * @param data Data string to publish
  * @retval HAL status
  */
HAL_StatusTypeDef ESP32_MQTT_Publish(const char* topic, const char* data) {
    char cmd_buffer[256]; // Adjust size as needed
    HAL_StatusTypeDef status;
    int data_len = strlen(data);

    if (data_len <= 0) {
        printf("MQTT Publish Error: Empty data\r\n");
        return HAL_ERROR;
    }

    printf("Publishing MQTT: Topic=[%s], Data=[%s]\n", topic, data);

    // Publish Data using MQTTPUBRAW (often more reliable for varied data)
    // Format: AT+MQTTPUBRAW=<LinkID>,<"Topic">,<Length>,<QoS>,<Retain>
    // QoS=0, Retain=0 are common defaults for sensor data
    // !! VERIFY SYNTAX with your specific ESP32 AT Firmware version !!

    // 1. Send the command indicating data length, wait for ">" prompt
    sprintf(cmd_buffer, "AT+MQTTPUBRAW=0,\"%s\",%d,0,0\r\n", topic, data_len);
    status = ESP32_SendCommand(cmd_buffer, ">", 5000); // Wait for prompt
    if (status != HAL_OK) {
        printf("MQTTPUBRAW command failed or no prompt '>'.\r\n");
        return status;
    }

    // 2. Send the actual data payload, wait for final "OK"
    // Note: Send command expects the exact number of bytes specified by data_len.
    // The 'SendCommand' here sends data and expects "OK". Ensure it doesn't add extra \r\n.
    // A dedicated function for sending raw data after ">" might be better.
    // For simplicity now, using SendCommand but it needs care.
    printf("Sending RAW data: %s\r\n", data);
    HAL_Delay(50); // Short delay sometimes helps before sending raw data
    status = ESP32_SendCommand(data, "OK", MQTT_PUB_TIMEOUT); // Send data, wait for final OK
    if (status != HAL_OK) {
        printf("MQTTPUBRAW data send failed or no OK received.\r\n");
        // It's possible the data sent successfully but OK wasn't parsed, check Adafruit IO!
        return status;
    }

    printf("MQTT Published.\r\n");
    // Note: Module might send unsolicited "+MQTTPUB: OK" message too.
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
    // Format: AT+MQTTDISCONN=<LinkID>
    // !! VERIFY SYNTAX with your specific ESP32 AT Firmware version !!
    status = ESP32_SendCommand("AT+MQTTDISCONN=0\r\n", "OK", 5000);
    printf("MQTT Disconnect command sent.\r\n");
    // Note: Module might send unsolicited "+MQTTDISCONNECTED" message too.
    return status;
}
