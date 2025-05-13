# Smart Home Monitor (ESP32-C6 + NPM1100)

## Hardware Setup
1. **Components**:
   - ESP32-C6 (bare chip)
   - BME280 Sensor (I2C)
   - NPM1100 PMIC (I2C)
   - 40MHz Crystal
   - LiPo Battery (JST PH connector)
   - USB-C for programming/power

2. **Schematic**: Refer to `SHM_V2.0.pdf` for connections.

## Firmware Setup
1. **Prerequisites**:
   - ESP-IDF v5.0+
   - Python dependencies: `esptool`, `idf.py`

2. **Build & Flash**:
   ```bash
   git clone https://github.com/your-repo/smart-home-monitor.git
   cd smart-home-monitor
   idf.py set-target esp32c6
   idf.py menuconfig  # Configure WiFi/MQTT settings
   idf.py build
   idf.py -p /dev/ttyUSB0 flash monitor
