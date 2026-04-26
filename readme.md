# Zigbee Smart Home Weather Station
<img width="496" height="481" alt="Display" src="https://github.com/user-attachments/assets/aff7b2b2-6a6c-4b60-a16e-d0e7e5b77ac7" /><img width="759" height="709" alt="System" src="https://github.com/user-attachments/assets/79aff351-bf34-4fa8-8650-9f258def7ced" />

A prototype wireless communication system for a smart home environment. This project utilizes Zigbee (802.15.4) modules in API mode to network multiple sensor nodes with a central coordinator and a graphical display.

## Project Overview
The system consists of three distinct end-devices and a central coordinator:
* Node 1 (Inside): Monitors indoor temperature and humidity using a DHT11 sensor.
* Node 2 (Outside): Monitors outdoor conditions using a DHT11 sensor.
* Node 3 (Display): Acts as a weather station, receiving data from other nodes and presenting it on an SSD1306 OLED display.
* Coordinator: Receives all network traffic via a Bluetooth/Zigbee bridge and logs data to a PC workstation.
<img width="764" height="474" alt="architecture" src="https://github.com/user-attachments/assets/dc4edd06-2f70-4fb6-a818-8634e5f61baf" />

## Technical Implementation
### Wireless Protocol: Zigbee API Mode
Unlike simple "Transparency Mode," this project uses API Mode, where data is wrapped in structured frames. This allows for:
* Mesh Networking: Increased reliability through self-healing and multiple data pathways.
* Addressing: Utilization of permanent 64-bit MAC addresses for unique device identification.
* Error Checking: Custom checksum validation to ensure data integrity during transmission.
### Data Encoding Strategy
To avoid the complexity of sending floating-point numbers over UART, a custom integer-offset method was implemented:
1. The float reading (e.g., 21.5°C) is multiplied by 10 (215).
2. An offset of 400 is added to ensure the value remains positive even in freezing temperatures (615).
3. The value is transmitted as an ASCII string with a prefix ('I' for Inside, 'U' for Ute/Outside).

## Hardware Components
Microcontrollers: 3x Arduino Uno.  
Wireless: 4x XBee S2C Modules (Zigbee).  
Sensors: 2x DHT11 Temperature & Humidity Sensors.  
Display: 1x SSD1306 OLED Display.  
Custom Enclosures: 3D-printed mounts and housings designed in Autodesk Fusion 360.  
  
Display Node:
<img width="472" height="703" alt="Receiver" src="https://github.com/user-attachments/assets/fe2e0c69-70b3-4152-b1cf-cddb2520810a" />
  
Sensor Nodes:
<img width="481" height="593" alt="sensor" src="https://github.com/user-attachments/assets/5d09cca4-0ecc-4734-8985-bbec910a05e4" />

## Software Architecture

### Embedded C
Located in XbeeSender.cpp and XbeeReceiver.cpp, the embedded code manages:
* Manual construction of XBee frames (Start delimiter 0x7E, Length, Frame Type, etc.).
* Checksum calculation using the formula: $0xFF - (\text{sum of bytes from frame type to checksum byte})$.

### Gateway & Logging (Python)
The bridge_logging.py script acts as the system gateway:
* Uses pyserial to communicate with the Zigbee coordinator.
* Parses incoming Type 0x81 (Receive Packet) frames. 
* Decodes the offset values and logs them to smart_home.txt with real-time timestamps

### How to Run
1. Configure XBees: Use X-CTU to set the PAN ID and ensure the function set is 802.15.4.
2. Flash Arduinos: Upload the sensor and display code via VS Code/PlatformIO.
3. Start Gateway: Run the Python script on your PC: python bridge_logging.py
