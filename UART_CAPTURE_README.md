# ESP32 UART Packet Sniffer

Capture WiFi and Ethernet packets from ESP32 and send them to your PC via UART (USB cable).

## How to Use

### 1. Build and Flash the ESP32

```bash
cd /home/junchannn/esp/hello_world
idf.py build flash
```

### 2. Run the Python Capture Script on Your PC

**Install pyserial first (if not already installed):**
```bash
pip install pyserial
```

**Run the capture script:**
```bash
python3 capture_uart_pcap.py /dev/ttyUSB0 captured_packets.pcap
```

Replace `/dev/ttyUSB0` with your actual serial port:
- Linux: Usually `/dev/ttyUSB0` or `/dev/ttyACM0`
- Find it with: `ls /dev/tty*` or `dmesg | grep tty`

### 3. View Captured Packets

Once you have captured some packets (you'll see packet count increasing), press Ctrl+C to stop.

**Open with Wireshark:**
```bash
wireshark captured_packets.pcap
```

## How It Works

1. **ESP32 side:**
   - Captures WiFi packets in promiscuous mode (channel 1)
   - Captures Ethernet packets (if hardware connected)
   - Formats packets as PCAP binary data
   - Sends via UART with magic markers: `<<<PCAP_START>>>` and `<<<PCAP_END>>>`

2. **PC side:**
   - Python script reads from serial port
   - Filters out ESP32 log messages
   - Extracts PCAP binary data between markers
   - Saves to `.pcap` file for Wireshark analysis

## Features

✅ No JTAG debugger needed - uses regular USB cable
✅ Captures both WiFi and Ethernet packets
✅ Real-time streaming to PC
✅ Log messages still visible on console
✅ Standard PCAP format compatible with Wireshark

## Troubleshooting

**"Permission denied" on serial port:**
```bash
sudo chmod 666 /dev/ttyUSB0
# Or add yourself to dialout group:
sudo usermod -a -G dialout $USER
# Then logout and login again
```

**No packets captured:**
- Make sure WiFi is scanning (check ESP32 logs)
- Verify correct WiFi channel (default is channel 1)
- Check that ESP32 printed "PCAP capture started" message

**Serial port not found:**
```bash
# Find your ESP32 port
ls -la /dev/tty* | grep USB
# or
dmesg | grep tty | tail
```

## Advanced Usage

**Capture to different file:**
```bash
python3 capture_uart_pcap.py /dev/ttyUSB0 my_capture.pcap
```

**Change WiFi channel (modify main.c):**
```c
sniffer_init(6, -1);  // Capture on channel 6 instead of 1
```

**Capture limited packets:**
```c
sniffer_init(1, 1000);  // Capture only 1000 packets then stop
```
