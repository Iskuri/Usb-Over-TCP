#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include "../inc/defines.h"
#include <libusb-1.0/libusb.h>
#include <unistd.h>
#include "signal.h"

int sockfd, portno, n;

void checkInt(int val) {

	close(sockfd);	
	exit(0);
}

// shows that gadget file is broken
void printBinaryGadgetDescriptor(unsigned char* gadgetData, int len) {

	printf("Gadget file: \n");
	
	uint8_t currSize = 0;
	uint8_t currInc = 0;
	for(int i = 0 ; i < len ; i++) {

		printf("%02x ",gadgetData[i]);
		//printf("DO GADGET FILE PRINT\n");
		if(gadgetData[i] != 0 && currSize == 0) {
			currSize = gadgetData[i];
			currInc = 0;
		}

		currInc++;

		if(currSize == currInc || i == 3) {
			currSize = 0;
			currInc = 0;
			printf("\n");
		}
	}
	printf("\n");
}

int createGadgetFileData(libusb_device* dev,unsigned char* deviceData) {

	printf("Creating gadget data\n");

	//unsigned char deviceData[65536];
	memset(deviceData,0x00,65536);
	
	struct libusb_device_descriptor descriptor;

	libusb_get_device_descriptor(dev, &descriptor);

	int offset = 4;

	for(int m = 0 ; m < 2 ; m++) {

		for(int i = 0 ; i  < descriptor.bNumConfigurations ; i++) {

			struct libusb_config_descriptor* config;
			libusb_get_config_descriptor(dev, i, &config);

			memcpy(&deviceData[offset],config,config->bLength);
			offset += (config->bLength);

			for(int j = 0 ; j < config->bNumInterfaces ; j++) {

				struct libusb_interface* interface;
				interface = (struct libusb_interface*)&config->interface[j];

				for(int k = 0 ; k < interface->num_altsetting ; k++) {
					memcpy(&deviceData[offset],&interface->altsetting[k],interface->altsetting[k].bLength);
					offset += interface->altsetting[k].bLength;

					//unsigned char* extraDescriptor = (unsigned char*)malloc(interface->altsetting[k].extra_length);
					//memcpy(extraDescriptor,interface->altsetting[k].extra,interface->altsetting[k].extra_length);
					memcpy(&deviceData[offset],interface->altsetting[k].extra,interface->altsetting[k].extra_length);
					offset += interface->altsetting[k].extra_length;

					printf("Extra length: %d\n",interface->altsetting[k].extra_length);

					for(int l = 0 ; l < interface->altsetting[k].bNumEndpoints ; l++) {

						struct libusb_endpoint_descriptor* endpoint;
						endpoint = (struct libusb_endpoint_descriptor*)&interface->altsetting[k].endpoint[l];
						memcpy(&deviceData[offset],endpoint,endpoint->bLength);
						offset += endpoint->bLength;
					}
				}
			}
		}
	}

	memcpy(&deviceData[offset],&descriptor,sizeof(struct libusb_device_descriptor));	
	offset += sizeof(struct libusb_device_descriptor);

	printBinaryGadgetDescriptor(deviceData,offset);
	return offset;
}

int sendData(int sockfd, unsigned char* data, int length) {

	int tries = 5;

	int status = -1;

	while(tries > 0) {

		int ret = write(sockfd,data,length);

		if(ret >= 0) {
			status = 0;
			break;
		}
	}

	return status;
}

int receiveData(int sockfd, unsigned char* data, int length) {

	int tries = 5;

	int status = -1;

	while(tries > 0) {

		int ret = read(sockfd,data,length);

		if(ret >= 0) {
			status = 0;
			break;
		}
	}

	return status;
}

int main(int argc, char *argv[]) {

	if(argc < 3) {

		printf("Usage: %s vid:pid ip_address\n",argv[0]);
		return 1;
	}

    srand (time(NULL));

    libusb_device** devices;
    libusb_device* device;
    libusb_context* context;
    struct libusb_device_handle* tryDeviceHandler;

    libusb_init(&context);
    int devicesCount = libusb_get_device_list(context, &devices);

    struct libusb_device_descriptor descriptor;
    libusb_device_handle* deviceHandler;

	uint16_t vid, pid;

	char* token = strtok (argv[1],":");
	vid = strtol(token,NULL,16);
	token = strtok(NULL,":");
	pid = strtol(token,NULL,16);

	if(vid == 0 || pid == 0) {
		printf("Invalid vid or pid\n");
		return 1;
	}

	printf("Got string: %04x:%04x\n",vid,pid);

	int retVal;
    for(int i = 0 ; i < devicesCount ; i++) {

	    retVal = libusb_open(devices[i], &tryDeviceHandler);

	    if(retVal < 0) {
	    	continue;
	    }

	    libusb_get_device_descriptor(devices[i], &descriptor);

	    printf("Found device: %04x:%04x\n",descriptor.idVendor,descriptor.idProduct);

	    if(descriptor.idVendor == vid && descriptor.idProduct == pid) {

	    	printf("Found chosen device\n");
	    	deviceHandler = tryDeviceHandler;
	    	device = devices[i];
	    	break;

	    }
    }

    if(!deviceHandler) {
    	printf("Couldn't find device\n");
    	return 1;
    }

    if(descriptor.bDeviceClass == LIBUSB_CLASS_HUB) {
    	printf("Cannot support hubs\n");
    	//return 1;
    }

    unsigned char deviceData[65536];
    int gadgetFileSize = createGadgetFileData(device,(unsigned char*)&deviceData);

    struct sockaddr_in serv_addr;
    struct hostent *server;

    signal(SIGINT, checkInt);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);    

    server = gethostbyname(argv[2]);

    printf("Starting socket connection\n");

    serv_addr.sin_family = AF_INET;

    bcopy((char *)server->h_addr,(char *)&serv_addr.sin_addr.s_addr,server->h_length);
    serv_addr.sin_port = htons(SERVER_PORT);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
        printf("Error connecting\n");
        return 1;
    }

	printf("In connection\n");

	int ret = write(sockfd,deviceData,gadgetFileSize);

	if(ret < 0) {
		printf("Writing descriptors failed\n");
		return 1;
	}

	unsigned char buff[65536];

    while(true) {

    	struct DataPacket dataRequest;

    	int ret = receiveData(sockfd, (unsigned char*)&dataRequest, sizeof(struct DataPacket));

    	unsigned char* dataBuff = (unsigned char*)malloc(dataRequest.length);

    	printf("Handling request on endpoint: %02x\n",dataRequest.endpoint);

    	if(dataRequest.endpoint == 0) {

    		if(dataRequest.bmRequestType&0x80) {
    			printf("Running read\n");
    			int readSize = read(sockfd,dataBuff,dataRequest.length);
    			printf("Finished read\n");
    		}
			int transferred = libusb_control_transfer (deviceHandler, dataRequest.bmRequestType, dataRequest.bRequest, dataRequest.wValue, dataRequest.wIndex, dataBuff, dataRequest.length, 255);

			printf("Libusb control transferred: %d\n",transferred);
    	
			if((dataRequest.bmRequestType&0x80) == 0) {
				printf("Running write\n");
				write(sockfd,dataBuff,dataRequest.length);
				printf("Written write\n");
			}

    	} else {

    		// in out &0x80
    		if(dataRequest.endpoint&0x80) {
    			int readSize = read(sockfd,dataBuff,dataRequest.length);
    		}

    		// check endpoint type

    		int transferred;
    		int interruptTransferVal = libusb_interrupt_transfer(deviceHandler,dataRequest.endpoint,dataBuff,dataRequest.length,&transferred,255);		
    		printf("Interrupt transfer value: %d\n",interruptTransferVal);

    		// write if needed
    		if((dataRequest.endpoint&0x80) == 0) {
    			write(sockfd,dataBuff,dataRequest.length);	
    		}

    	}

    	free(dataBuff);

    }

    close(sockfd);
}
