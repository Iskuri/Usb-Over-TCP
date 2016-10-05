#ifndef DEFINES_INC
#define DEFINES_INC

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>


#define VERSION 1

#define SERVER_PORT 7252

struct DataPacket {
	uint8_t endpoint;
	uint8_t bmRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t length;
};

#endif