#ifndef INC_CONFIG_H_
#define INC_CONFIG_H_

// --- WiFi Configuration ---
#define WIFI_SSID        "Your_WiFi_SSID"     // Replace with your WiFi SSID
#define WIFI_PASSWORD    "Your_WiFi_Password" // Replace with your WiFi Password

// --- Adafruit IO Configuration ---
#define AIO_USERNAME     "Your_AIO_Username" // Replace with your Adafruit IO Username
#define AIO_KEY          "Your_AIO_Key"      // Replace with your Adafruit IO Key (KEEP SECRET!)

#define MQTT_CLIENT_ID   "STM32F103Client" // Or any unique ID

// --- Adafruit IO Feed Names (Match what you created on io.adafruit.com) ---
// Note: Topic format will be AIO_USERNAME "/feeds/" FEED_NAME
#define AIO_TEMP_FEED_NAME   "temperature"
#define AIO_HUMID_FEED_NAME  "humidity"

// --- Peripheral Handles (Defined in main.c by CubeMX) ---
// Ensure these match the handles used in your main.c/CubeMX config
#define ESP32_UART_HANDLE huart1 // UART connected to ESP32
#define DEBUG_UART_HANDLE huart2 // Optional UART for printf debugging
#define SHT31_I2C_HANDLE  hi2c1  // I2C connected to SHT31

// --- Timing ---
#define UART_TIMEOUT        1000        // UART timeout in ms for basic AT commands
#define MQTT_CONNECT_TIMEOUT 10000      // Longer timeout for MQTT connection
#define MQTT_PUB_TIMEOUT    5000        // Timeout for MQTT publish
#define WIFI_CONNECT_TIMEOUT 20000      // Long timeout for WiFi connection
#define SENSOR_READ_INTERVAL 30000      // Read sensor every 30 seconds (30000 ms)


#endif /* INC_CONFIG_H_ */

