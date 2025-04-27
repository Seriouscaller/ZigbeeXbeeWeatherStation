#include <stdio.h>
#include <string.h>
#include <stdint.h>

const uint16_t NODE_INSIDE_ADRESS = 0x0013A2004214B69D;
const uint16_t NODE_OUTSIDE_ADRESS = 0x0013A2004214B6BD;
const uint16_t NODE_LCD_ADRESS = 0x0013A20041C72022;
const uint16_t COORDINATOR_ADDRESS = 0x0013A2004214ADFB;

typedef struct {
    uint8_t startdelimiter;
    uint8_t length;
    uint8_t type;
    uint8_t ID;

    uint8_t destinationAddress64bit[8];

    uint8_t payload[20];
    uint8_t payloadLength;

    uint8_t checksum;
}XBeeFrame;

enum sensorType {
    INSIDE = 0,
    OUTSIDE = 1,
};

void initializeFrame(XBeeFrame*, const uint8_t*);
void addDataToFrame(XBeeFrame*, sensorType, uint8_t);
uint16_t finalizeFrame(XBeeFrame*, uint8_t*, uint8_t);

void initializeFrame(XBeeFrame* frame, const uint16_t* destaddr64bit) {

    // Set all fields in frame to 0
    memset(frame, 0, sizeof(XBeeFrame));

    frame->startdelimiter = 0x7F;

    // Frametype ZigBee Transmit Request 64-bit Address
    frame->type = 0x00;
    frame->ID = 0x01;

    // Set destination, use 64bit addresses
    memcpy(frame->destinationAddress64bit, destaddr64bit, 8);

    frame->payloadLength = 0;
}

void addDataToFrame(XBeeFrame* frame, sensorType snsr_type, uint8_t snrs_value) {
    frame->payload[frame->payloadLength++] = snsr_type;
    frame->payload[frame->payloadLength++] = snrs_value;
}

uint8_t calculateChecksum(XBeeFrame* frame) {
    uint8_t sum = 0;

    sum += (frame->type + frame->ID);
    for (int i = 0; i < 8; i++) {
        sum += frame->destinationAddress64bit[i];
    }

    for (int i = 0; i < frame->payloadLength; i++) {
        sum += frame->payload[i];
    }
    return (0xFF - sum);
}

uint16_t finalizeFrame(XBeeFrame* frame, uint8_t* buffer, uint8_t bufferSize) {
    // 10 is coming from type, id, destaddr, options summed together
    int16_t length = 10 + frame->payloadLength;
    uint8_t index = 0;

    // Enough room in buffer? start + length + data + checksum
    if (bufferSize < (3 + length + 1)) {
        return 0;
    }

    int8_t checksum = calculateChecksum(frame);

    buffer[index++] = frame->startdelimiter;

    // MSB
    buffer[index++] = length >> 8;
    // LSB. Cuts off upper byte of length
    buffer[index++] = length & 0xFF;

    buffer[index++] = frame->type;
    buffer[index++] = frame->ID;

    // 64-bit address (big end)
    for (int i = 0; i < 8; i++) {
        buffer[index++] = frame->destinationAddress64bit[i];
    }

    // No options
    buffer[index++] = 0x00;

    // Sensordata being sent (big end)
    for (int i = 0; i < 8; i++) {
        buffer[index++] = frame->payload[i];
    }

    buffer[index++] = checksum;

    return index;
}

void printFrame(XBeeFrame* frame) {
    printf("XBee Data-Frame:");
    printf("Length: %d\n", frame->length);
    printf("Frame-Type: 0x%02x\n", frame->type);
    printf("Frame-ID: 0x%02x\n", frame->ID);

    printf("Destination Address 64-bit:\n");
    for (int i = 0; i < 8; i++) {
        printf("%02x ", frame->destinationAddress64bit[i]);
    }
    printf("\n");

    printf("Payload-Length: %d\n", frame->payloadLength);
    printf("Payload:\n");

    for (int i = 0; i < frame->payloadLength; i++) {
        printf("%02x ", frame->payload[i]);
    }
    printf("\n");

    printf("Checksum: 0x%02x\n", frame->checksum);
}

void printBuffer(const uint8_t* buffer, uint8_t size) {
    printf("Buffer Contents:\n");
    printf("Size: %d bytes\n", size);

    for (int i = 0; i < size; i++) {
        printf("%02X ", buffer[i]);
    }
    printf("\n");

}

int main(void) {
    XBeeFrame frame;
    const uint8_t BUFFER_SIZE = 100;
    uint8_t buffer[BUFFER_SIZE];

    initializeFrame(&frame, &COORDINATOR_ADDRESS);
    addDataToFrame(&frame, INSIDE, 48);
    finalizeFrame(&frame, buffer, BUFFER_SIZE);

    printBuffer(buffer, 30);
    printFrame(&frame);

    return 0;
}