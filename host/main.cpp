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

int sockfd, portno, n;

void checkInt(int val) {

	close(sockfd);	
	exit(0);
}

int main(int argc, char *argv[]) {

	if(argc < 3) {

		printf("Usage: %s vid:pid ip_address\n",argv[0]);
		return 1;
	}

    srand (time(NULL));

    libusb_device** devices;
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

	unsigned char gadgetFileData[1024];

	memset(gadgetFileData,0x00,1024);

	memcpy(&gadgetFileData,&descriptor,sizeof(struct libusb_device_descriptor));	

	int ret = write(sockfd,gadgetFileData,500);

	if(ret < 0) {
		printf("Writing descriptors failed\n");
		return 1;
	}

    while(true) {




    }

    close(sockfd);
}
