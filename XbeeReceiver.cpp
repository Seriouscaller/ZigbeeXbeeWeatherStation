#include <stdio.h>
#include <string.h>
#include <stdint.h>

const uint64_t NODE_INSIDE_ADRESS = 0x0013A2004214B69D;
const uint64_t NODE_OUTSIDE_ADRESS = 0x0013A2004214B6BD;
const uint64_t NODE_LCD_ADRESS = 0x0013A20041C72022;
const uint64_t COORDINATOR_ADDRESS = 0x0013A2004214ADFB;

#define START_DELIMITIER 0x7E
#define ZIGBEE_RECEIVE_PACKET_0x90 0x90

typedef struct {
    uint8_t startdelimiter;
    uint8_t length;
    uint8_t type;
    uint8_t ID;

    uint8_t sourceAddress64bit[8];
    uint8_t sourceAddress16bit[2];
    uint8_t options;

    uint8_t payload[100];
    uint8_t payloadLength;

    uint8_t checksum;
    bool valid;
}XBeeReceiverFrame;

enum sensorType {
    INSIDE = 0,
    OUTSIDE = 1,
};

bool receiveFrame(XBeeReceiverFrame*, const uint8_t*, uint8_t);
uint8_t validateChecksum(XBeeReceiverFrame*, const uint8_t*, uint16_t);
void printReceivedFrame(XBeeReceiverFrame*);

bool receiveFrame(XBeeReceiverFrame* frame, const uint8_t* RX_buffer, uint8_t RX_buffer_size) {
    // Initialize the frame to all 0s
    memset(frame, 0, sizeof(XBeeReceiverFrame));
    frame->valid = false;

    // Too small frame, or no startdelimiter at beginning of buffer -> Abort
    if (RX_buffer_size < 6 || RX_buffer[0] != START_DELIMITIER) {
        printf("Invalid Frame! Aborting!\n");
        return false;
    }

    uint8_t index = 0;

    frame->startdelimiter = RX_buffer[index++];

    // Since length is retreived from 2 separate bytes, and stored into a 16-bit uint,
    // we need to bit-shift and do bitwise OR
    frame->length = (RX_buffer[index] << 8 | (RX_buffer[index + 1]));

    index = 2;

    // Length of the frame + (start (1) + length (2) + checksum (1))
    if (RX_buffer_size < (frame->length + 4)) {
        printf("Insufficient buffersize! Aborting!\n");
        return false;
    }

    // Any-other frametypes gets discarded
    if (frame->type != ZIGBEE_RECEIVE_PACKET_0x90) {
        printf("Unknown frametype! Aborting!\n");
        return false;
    }
    // Proceed if frametype is a ZIGBEE RECEIVE PACKET 0x90

    // Retrieving 64-bit address (8 bytes)
    for (int i = 0; i < 8; i++){
        frame->sourceAddress64bit[i] = RX_buffer[index++];
    }

    // Retrieving 16-bit address (2 bytes)
    for (int i = 0; i < 2; i++) {
        frame->sourceAddress16bit[i] = RX_buffer[index++];
    }

    frame->options = RX_buffer[index++];

    // Length is everything from frametype to before checksum
    // 12 is values inside length that is not part of payload (frametype + ID + 64, 16 bit addresses + options)
    frame->payloadLength = frame->length - 12;

    for (int i = 0; i < frame->payloadLength; i++){
        frame->payload[i] = RX_buffer[index++];
    }

    frame->checksum = RX_buffer[index++];

    if (!validateChecksum(frame, RX_buffer, RX_buffer_size)) {
        printf("Invalid Checksum! Aborting!\n");
        return false;
    }

    frame->valid = true;
    return true;
}

uint8_t validateChecksum(XBeeReceiverFrame* frame, const uint8_t* RX_buffer, uint16_t RX_buffer_size) {
    uint8_t sum = 0;
    
    // 0 startdelim
    // 1 length MSB
    // 2 length LSB
    // 3 type (starting here)
    // 4 ...
    // X Checksum (stop before checksum)
    for (int i = 3; i < RX_buffer_size - 1; i++)
    {
        sum += RX_buffer[i];
    }

    // Our checksumfunction ->  return (0xFF - sum);
    // If previous checksum + our new sum is equal to 255, then the checksum is correct -> Data unchanged
    return (frame->checksum + sum == 0xFF);
}

void printReceivedFrame(XBeeReceiverFrame* frame) {
    printf("XBee Data-Frame:");
    printf("Length: %d\n", frame->length);
    printf("Frame-Type: 0x%02x\n", frame->type);
    printf("Frame-ID: 0x%02x\n", frame->ID);

    printf("Destination Address 64-bit:\n");
    for (int i = 0; i < 8; i++) {
        printf("%02x ", frame->sourceAddress64bit[i]);
    }
    printf("\n");

    printf("Destination Address 16-bit:\n");
    for (int i = 0; i < 2; i++) {
        printf("%02x ", frame->sourceAddress16bit[i]);
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

int main(void) {
    XBeeReceiverFrame frame;
    const uint8_t BUFFER_SIZE = 100;
    uint8_t buffer[BUFFER_SIZE];

    printReceivedFrame(&frame);

    return 0;
}