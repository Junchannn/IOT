#!/usr/bin/env python3
"""
UART PCAP Capture Script with Real-time Analysis
Continuously captures and parses PCAP data from ESP32 via UART
"""

import serial
import sys
import time
from datetime import datetime
import struct

# Configuration
SERIAL_PORT = '/dev/ttyUSB0'
BAUD_RATE = 115200
OUTPUT_FILE = '/mnt/c/School/captured_packets.pcap'

# PCAP constants
PCAP_MAGIC = 0xa1b2c3d4
PCAP_HEADER_SIZE = 24
PCAP_PACKET_HEADER_SIZE = 16

# Packet framing markers
PKT_START_MARKER = 0xAAAAAAAA
PKT_END_MARKER = 0x55555555

def parse_802_11_frame(data):
    """Parse basic 802.11 WiFi frame information"""
    if len(data) < 24:
        return None
    
    # Frame Control (2 bytes)
    frame_control = struct.unpack('<H', data[0:2])[0]
    frame_type = (frame_control >> 2) & 0x03
    frame_subtype = (frame_control >> 4) & 0x0F
    
    # Frame types
    types = {0: "Management", 1: "Control", 2: "Data"}
    frame_type_name = types.get(frame_type, "Unknown")
    
    # Try to extract addresses (depends on frame type)
    try:
        addr1 = ':'.join(f'{b:02x}' for b in data[4:10])
        addr2 = ':'.join(f'{b:02x}' for b in data[10:16])
        addr3 = ':'.join(f'{b:02x}' for b in data[16:22])
        return {
            'type': frame_type_name,
            'subtype': frame_subtype,
            'length': len(data),
            'addr1': addr1,
            'addr2': addr2,
            'addr3': addr3
        }
    except:
        return {
            'type': frame_type_name,
            'subtype': frame_subtype,
            'length': len(data)
        }

def parse_pcap_packet(data):
    """Parse PCAP packet record"""
    if len(data) < 16:
        return None
    
    # PCAP packet header: ts_sec, ts_usec, incl_len, orig_len
    ts_sec, ts_usec, incl_len, orig_len = struct.unpack('<IIII', data[0:16])
    packet_data = data[16:16+incl_len]
    
    # Parse 802.11 frame
    frame_info = parse_802_11_frame(packet_data)
    
    return {
        'timestamp': f"{ts_sec}.{ts_usec:06d}",
        'length': incl_len,
        'frame': frame_info
    }

def main():
    if len(sys.argv) > 1:
        port = sys.argv[1]
    else:
        port = SERIAL_PORT
    
    if len(sys.argv) > 2:
        output_file = sys.argv[2]
    else:
        output_file = OUTPUT_FILE
    
    print(f"ESP32 UART PCAP Capture")
    print(f"=======================")
    print(f"Port: {port} @ {BAUD_RATE}")
    print(f"Output: {output_file}\n")
    
    ser = None
    pcap_file = None
    
    try:
        # Open serial port
        ser = serial.Serial(port, BAUD_RATE, timeout=0.5)
        ser.reset_input_buffer()
        
        # Open PCAP output file
        pcap_file = open(output_file, 'wb')
        
        # State
        buf = bytearray()
        pkt_count = 0
        header_written = False
        
        print("Waiting for packets...\n")
        
        while True:
            # Read chunk
            chunk = ser.read(4096)
            if chunk:
                buf.extend(chunk)
            
            # Process packets with markers
            while True:
                # Look for start marker
                if len(buf) < 4:
                    break
                
                try:
                    # Find start marker
                    idx = buf.index(struct.pack('<I', PKT_START_MARKER))
                    buf = buf[idx:]  # Skip to marker
                except ValueError:
                    # No marker found, keep last 3 bytes
                    buf = buf[-3:] if len(buf) > 3 else bytearray()
                    break
                
                # Check if we have: start_marker(4) + length(4)
                if len(buf) < 8:
                    break
                
                # Read length
                pkt_len = struct.unpack('<I', buf[4:8])[0]
                
                # Sanity check
                if pkt_len > 65535 or pkt_len == 0:
                    buf = buf[1:]  # Skip bad marker
                    continue
                
                # Total frame: start(4) + len(4) + pcap_hdr(16) + payload(pkt_len) + end(4)
                total = 8 + 16 + pkt_len + 4
                
                if len(buf) < total:
                    break  # Wait for complete frame
                
                # Verify end marker
                end_marker_pos = total - 4
                end_marker = struct.unpack('<I', buf[end_marker_pos:end_marker_pos+4])[0]
                
                if end_marker != PKT_END_MARKER:
                    buf = buf[1:]  # Bad frame, skip
                    continue
                
                # Extract PCAP data (skip start marker and length)
                pcap_data = buf[8:end_marker_pos]
                
                # Write PCAP header on first packet
                if not header_written:
                    # Create PCAP file header
                    header = struct.pack('<IHHiIII',
                        PCAP_MAGIC,  # magic
                        2, 4,        # version major, minor
                        0,           # timezone
                        0,           # sigfigs
                        65535,       # snaplen
                        105          # linktype (802.11)
                    )
                    pcap_file.write(header)
                    header_written = True
                    print(f"[{datetime.now().strftime('%H:%M:%S')}] Capturing...\n")
                
                # Write packet
                pcap_file.write(pcap_data)
                pkt_count += 1
                
                # Display
                if pkt_count % 50 == 0:
                    # Quick parse
                    if len(pcap_data) > 24:
                        frame = pcap_data[16:]
                        if len(frame) >= 16:
                            fc = struct.unpack('<H', frame[0:2])[0]
                            ftype = ["Mgmt", "Ctrl", "Data"][(fc >> 2) & 0x03]
                            addr2 = ':'.join(f'{b:02x}' for b in frame[10:16])
                            print(f"\r#{pkt_count:5d} | {ftype:4s} | {pkt_len:4d}B | {addr2}", 
                                  end='', flush=True)
                
                # Periodic flush
                if pkt_count % 100 == 0:
                    pcap_file.flush()
                
                # Remove processed frame
                buf = buf[total:]
                
    except KeyboardInterrupt:
        print(f"\n\nStopped. Captured {pkt_count} packets → {output_file}")
    
    except Exception as e:
        print(f"\nError: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
    
    finally:
        if pcap_file:
            pcap_file.close()
        if ser and ser.is_open:
            ser.close()

if __name__ == '__main__':
    main()
