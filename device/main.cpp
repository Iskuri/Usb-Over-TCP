#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include "../inc/defines.h"
#include <fcntl.h>
#include <linux/usb/ch9.h>
#include <poll.h>
#include <linux/usb/gadgetfs.h>
#include <sys/types.h>
#include <iostream>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include <cstring>
#include <sys/mount.h>
#include "signal.h"

bool run = true;

int sockfd, newsockfd, portno;

int gadgetFile;

static pthread_t gadgetThread;
static pthread_t readerThread;
static pthread_mutex_t deviceMutex;

void setupPacket(struct DataPacket* dataPacket, struct usb_ctrlrequest *setup, uint8_t endpoint) {

	dataPacket->endpoint = endpoint;
	dataPacket->bmRequestType = setup->bRequestType;
	dataPacket->bRequest = setup->bRequest;
	dataPacket->wValue = setup->wValue;
	dataPacket->wIndex = setup->wIndex;
	dataPacket->length = setup->wLength;	
}

void sendTransaction(struct usb_ctrlrequest *setup, uint8_t endpoint, unsigned char* buff, uint16_t length) {

	pthread_mutex_lock(&deviceMutex);

	struct DataPacket dataPacket;

	setupPacket(&dataPacket,setup,endpoint);

	write(newsockfd,(unsigned char*)&dataPacket,sizeof(struct DataPacket));

	write(newsockfd,buff,length);

	pthread_mutex_unlock(&deviceMutex);

}

void receiveTransaction(struct usb_ctrlrequest *setup, uint8_t endpoint,unsigned char* buff,uint16_t length) {

	pthread_mutex_lock(&deviceMutex);

	struct DataPacket dataPacket;

	setupPacket(&dataPacket,setup,endpoint);

	write(newsockfd,(unsigned char*)&dataPacket,sizeof(struct DataPacket));

	read(newsockfd,buff,length);

	pthread_mutex_unlock(&deviceMutex);
}

static void handleSetup(struct usb_ctrlrequest *setup) {
	
	uint16_t value = __le16_to_cpu(setup->wValue);
	uint16_t index = __le16_to_cpu(setup->wIndex);
	uint16_t length = __le16_to_cpu(setup->wLength);

	printf("Got USB bRequest: %d(%02x) with type %d(%02x) of length %d\n",setup->bRequest,setup->bRequest,setup->bRequestType, setup->bRequestType, setup->wLength);

	// start transactions

	unsigned char* buf = (unsigned char*)malloc(length);

	if(setup->bRequestType&0x80) {

		/* get buff */
		receiveTransaction(setup,0x00,buf,length);
		write(gadgetFile, buf, length);

	} else {

		/* send buff */
		read(gadgetFile, buf, length);
		sendTransaction(setup,0x00,buf,length);

	}

	free(buf);

}

static void* gadgetCfgCb(void* nothing) {

	struct usb_gadgetfs_event events[5];

	struct pollfd pollRecv;
    pollRecv.fd=gadgetFile;
    pollRecv.events=POLLIN | POLLOUT | POLLHUP;;

    printf("Starting gadget read\n");

	char recvData[32];
	int readData;
	
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

	while(1) {
		
		// printf("At gadget polling\n");

		int pollVal = poll(&pollRecv,1,500);

		if(pollVal >= 0) {
		// if(pollVal >= 0 && (pollRecv.revents&POLLIN)) {

			int ret = read(gadgetFile,&events,sizeof(events));

			// printf("Number of events: %d\n",(ret / sizeof(events[0])));
			
			unsigned char* eventData = (unsigned char*)malloc(sizeof(events[0]));
			
			for(int i = 0 ; i < (ret / sizeof(events[0])) ; i++) {

				// printf("Event type: %d\n",events[i].type);
				
				switch(events[i].type) {

					case GADGETFS_SETUP:

						handleSetup(&events[i].u.setup);

						break;
					case GADGETFS_NOP:
						break;
					case GADGETFS_CONNECT:
						printf("Connect\n");
						break;
					case GADGETFS_DISCONNECT:
						printf("Disconnect\n");
						break;
					case GADGETFS_SUSPEND:
						printf("Suspend\n");
						break;
					default:
						printf("Unknown type: %d\n",events[i].type);
						exit(0);
						break;
				}

			}
		}

	}
}

void checkInt(int val) {

	printf("Checking int\n");
	run = false;
	close(newsockfd);
	close(sockfd);	
	exit(0);
}

int main(int argc, char *argv[]) {

	socklen_t clilen;
	char buffer[65536];

	char deviceData[65536];

	struct sockaddr_in serv_addr, cli_addr;
	int n;

    signal(SIGINT, checkInt);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)  {
		printf("Couldn't open socket\n");	
		return 1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(SERVER_PORT);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {

		printf("Couldn't bind socket\n");
		return 1;
	}

	int ret = listen(sockfd,5);

	printf("Listen got ret: %d\n",ret);

	while(run) {

		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, 
			(struct sockaddr *) &cli_addr, 
			&clilen);

		printf("Accepted connection\n");

		int gadgetFileSize = read(newsockfd,buffer,65536);
		printf("Read gadget file size %d\n",gadgetFileSize);

		// mount files here
		// write to gadget file
		// write to open endpoint

		// start waiting on gadget requests

		mkdir("/dev/gadget/",455);
		umount2("/dev/gadget/", MNT_FORCE);
		int mountRet = mount("none", "/dev/gadget/", "gadgetfs", 0, "");

		if(mountRet < 0) {
			printf("Mounting gadget failed\n");
			break;
		}

		gadgetFile = open("/dev/gadget/musb-hdrc", O_RDWR);

		if(gadgetFile < 0) {
			printf("Could not open gadget file, got response %d\n", gadgetFile);
			break;
		}

		int writeValGadget = write(gadgetFile,buffer,gadgetFileSize); // make sure length is right
		
		pthread_create(&gadgetThread,0,gadgetCfgCb,NULL);



	}

	printf("Exiting\n");

	close(newsockfd);
	close(sockfd);
}
