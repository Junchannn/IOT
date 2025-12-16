#!/bin/bash
# Script to save WiFi and MQTT credentials to ESP32 NVS flash

echo "ESP32 WiFi & MQTT Configuration Tool"
echo "======================================"

read -p "Enter WiFi SSID: " WIFI_SSID
read -sp "Enter WiFi Password: " WIFI_PASS
echo ""
read -p "Enter MQTT Broker URL (default: mqtt://broker.hivemq.com): " MQTT_URL
MQTT_URL=${MQTT_URL:-mqtt://broker.hivemq.com}

echo ""
echo "Credentials to be stored:"
echo "  WiFi SSID: $WIFI_SSID"
echo "  WiFi Pass: ********"
echo "  MQTT URL: $MQTT_URL"
echo ""
read -p "Confirm? (y/n): " -n 1 -r
echo ""

if [[ $REPLY =~ ^[Yy]$ ]]; then
    # Create Python script to write to NVS
    cat > /tmp/nvs_write.py << EOF
import sys
sys.path.append('$IDF_PATH/components/nvs_flash/nvs_partition_generator')
from nvs_partition_gen import *

nvs_data = [
    ['storage', 'data', 'wifi_ssid', 'string', '$WIFI_SSID'],
    ['storage', 'data', 'wifi_pass', 'string', '$WIFI_PASS'],
    ['storage', 'data', 'mqtt_url', 'string', '$MQTT_URL']
]

# Generate NVS partition binary
with open('credentials.csv', 'w') as f:
    f.write('key,type,encoding,value\\n')
    for item in nvs_data:
        f.write(f'storage,namespace,,\\n')
        f.write(f'{item[2]},{item[3]},string,{item[4]}\\n')
        break  # Only write once

# Run the actual generation
import subprocess
subprocess.run([
    'python', '$IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py',
    'generate', 'credentials.csv', 'credentials.bin', '0x4000'
])
EOF

    python3 /tmp/nvs_write.py
    
    echo ""
    echo "Flashing credentials to ESP32..."
    read -p "Enter serial port (e.g., /dev/ttyUSB0): " SERIAL_PORT
    
    esptool.py --port $SERIAL_PORT write_flash 0x9000 credentials.bin
    
    echo "Done! Credentials saved to ESP32 NVS."
    rm /tmp/nvs_write.py credentials.csv credentials.bin
else
    echo "Cancelled."
fi
