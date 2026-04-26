import serial
import time
import datetime
import logging
from typing import Tuple

"""
Bluetooth works if I select COM9 'Outgoing', even though I'm receiving data

We have a range from 00.0 up to 99.0 using two chars
To be able to have one decimal precision, without floating-point number,
we add one digit in the beginning of the temperature, keeping the temperature as an integer.
Lets choose our reference (average temp/middle-temp) of 10.0. Now we shall convert 10.0 to 500.
For each measurement, we add 400 to the value, before we transmit.

However, if we receive a value of 500 from our sensor,
it means the actual temperature is 10.0 degrees.

If we receive a value of 521 from our sensor, it means the actual temperature
is 12.1 degrees

-400 000
-300 100
-200 200   = -20.0 dgr
-100 300
 000 400
 100 500   = 10.0 dgr
 200 600
 300 700
 400 800   = 40.0 dgr
 500 900
"""

'''
    OUTGOING:
    Type 00, broadcast, payload: 'U621'
    7E 00 0F 00 01 00 00 00 00 00 00 FF FF 00 55 36 32 31 12
    
    INCOMING:
    Type 81, received, payload: 'U621'
    7E 00 09 81 FF FE 11 01 55 36 32 31 81
'''

class ZigbeeFrame:
    START_DELIMITER = 0x7E
    RECEIVE_PACKET_TYPE = 0x81

    @staticmethod
    def extract_rx64_payload(frame_bytes: bytes) -> bytes:
        """
        Extracts payload from a type80 frame

        Arguments:
            A complete zigbee frame as bytes

        Returns:
            The payload of the frame

        Raises:
            ValueError: If the frame is invalid
        """

        # Does the frame have a valid startbyte?
        if not frame_bytes or frame_bytes[0] != ZigbeeFrame.START_DELIMITER:
            raise ValueError("Invalid frame!")

        # Length is store in position 1 & 2. 2 Bytes
        # Normally length 9 (between length and checksum). (Startdelim + length + checksum = 4)
        # Total frame-length 9+4 = 13B sent
        length = (frame_bytes[1] << 8) + frame_bytes[2]

        # Checksum is stored in the last byte.
        checksum_pos = 3 + length

        if len(frame_bytes) < checksum_pos:
            raise ValueError("The frame is too short!")

        if frame_bytes[3] != ZigbeeFrame.RECEIVE_PACKET_TYPE:
            raise ValueError(f"Not a type 81 frame! Got a {frame_bytes[3]} instead.")

        #start 3 -> startdelim1 + length2
        frame_data = frame_bytes[3:3+length]

        calculated_checksum = 0xFF - (sum(frame_data) & 0xFF)
        recieved_checksum = frame_bytes[checksum_pos]

        if calculated_checksum != recieved_checksum:
            raise ValueError("Calculated checksum does not match!")

        # Format of Type-81:
        # startdel1 + len2 + type1 + ID1 + srcaddr8 + rssi1 + opt1 = 14B
        payload = frame_bytes[8:3+length]

        return payload

class SensorData:
    def __init__(self, sensor_value_offset: int = 400):
        self.sensor_value_offset = sensor_value_offset

    def process_payload(self, payload: bytes) -> Tuple[str, str]:
        """
        Processes the payload, to extract the sensor-data

        Args:
            The payload as bytes (immutable, unchangeable)

        Returns:
            Returns the sensor-type (I or U) and sensor-reading (with 400-offset) as a tuple
        """
        try:
            ascii_payload = payload.decode('ascii')

            # Extract either 'I' (inside) temp or 'U' (outside) temp
            sensor_type = ascii_payload[0:1]

            # Reading with added 400 offset
            sensor_reading = ascii_payload[1:]

            logging.debug(f"Sensor Type: {sensor_type}, Sensor Value: {sensor_reading}")
            return sensor_type, sensor_reading

        except UnicodeDecodeError:
            logging.error("Error converting payload to ASCII")
            return None, None

    def restore_value_from_offset(self, sensor_value: str) -> float:
        """
        Restoring the actual reading of the tempsensor from our offset value

        Args:
            The sensor value as a string

        Returns:
            The sensor-reading as a float. One decimal precision

        """

        # Here we go from 521 (12.1 C) - 400 -> 121
        value_int = int(sensor_value) - self.sensor_value_offset

        # Moving decimal place one step. 121 -> 12.1 degr C
        value_float = float(value_int) / 10
        return value_float

class ZigbeeLogger:
    """Class to handle bluetooth logging"""
    def __init__(self, port: str, baudrate: int, log_file: str, sensor_value_offset: int = 400):
        self.port = port
        self.baudrate = baudrate
        self.log_file = log_file
        self.sensor_handler = SensorData(sensor_value_offset)
        self.serial_conn = None

    def connect(self):
        try:
            self.serial_conn = serial.Serial(port=self.port, baudrate=self.baudrate, timeout=1)
            logging.info(f"Connected to {self.port} at {self.baudrate} baud")
        except serial.SerialException as e:
            logging.error(f"Failed to connect to {self.port}: {e}")

    def disconnect(self):
        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.close()
            logging.info("Serial connection closed")

    def process_frame(self, frame_data: bytes) -> Tuple[str, float]:
        """
        Class to handle the entire processing of out zigbee frame

        Args:
            The entire zigbee frame as bytes

        Returns:
            Returns the sensor-type (I or U) and sensor-reading as a tuple, or (None, None) if the processing fails
        """
        try:
            payload = ZigbeeFrame.extract_rx64_payload(frame_data)
            logging.debug(f"Extracted payload (hex): {payload.hex()}")

            sensor_type, sensor_reading = self.sensor_handler.process_payload(payload)

            if sensor_type and sensor_reading:
                processed_value = self.sensor_handler.restore_value_from_offset(sensor_reading)
                return sensor_type, processed_value
        except ValueError as e:
            logging.error(f"Error processing frame: {e}")
            return None, None

    def run(self):
        """Our main method"""
        try:
            # Start the bluetooth-connection
            self.connect()

            with open(self.log_file, "w") as log_file:
                logging.info(f"Logging started to {self.log_file}")

                while True:

                    # Only respond to packets larger than 10 Bytes
                    if self.serial_conn.in_waiting > 10:

                        # Read data from bluetooth
                        data = self.serial_conn.read(self.serial_conn.in_waiting)
                        logging.debug(f"Received {len(data)} bytes")
                        logging.debug(f"Data: {data.hex()}")
                        # Extract the sensor-data
                        sensor_type, processed_value = self.process_frame(data)

                        if sensor_type and sensor_type is not None:
                            # Timestamp has the format 2025-05-07 20:15:01
                            timestamp = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')

                            # Final format in the log-file. [2025-05-07 20:15:01] U 12.3
                            log_entry = f"[{timestamp}] {sensor_type} {processed_value:.1f}]"
                            log_file.write(log_entry + "\n")
                            log_file.flush()

                            logging.info(f"Logged {log_entry}")

                    time.sleep(0.01)
        except KeyboardInterrupt:
            logging.info("Logging stopped by user")
        except Exception as e:
            logging.error(f"Unexpected error during logging: {e}")
        finally:
            self.disconnect()

def main():
    # Here we set what level of logging we are using. DEBUG will log everything. Final version = INFO
    logging.basicConfig(
        level=logging.DEBUG,
        format="%(asctime)s - %(levelname)s - %(message)s"
    )

    PORT = 'COM9'
    BAUD_RATE = 9600
    LOG_FILE = 'smart_home.txt'
    SENSOR_VALUE_OFFSET = 400

    logger = ZigbeeLogger(PORT, BAUD_RATE, LOG_FILE, SENSOR_VALUE_OFFSET)
    logger.run()

if __name__ == '__main__':
    main()