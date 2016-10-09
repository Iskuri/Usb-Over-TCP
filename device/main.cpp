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
static pthread_t epThreads[32];
static pthread_mutex_t deviceMutex;

void setupPacket(struct DataPacket* dataPacket, struct usb_ctrlrequest *setup, uint8_t endpoint) {

	dataPacket->endpoint = endpoint;
	dataPacket->bmRequestType = setup->bRequestType;
	dataPacket->bRequest = setup->bRequest;
	dataPacket->wValue = setup->wValue;
	dataPacket->wIndex = setup->wIndex;
	dataPacket->length = setup->wLength;	
}

int sendTransaction(struct usb_ctrlrequest *setup, uint8_t endpoint, unsigned char* buff, uint16_t length) {

	pthread_mutex_lock(&deviceMutex);

	struct DataPacket dataPacket;

	memset(&dataPacket,0x00,sizeof(struct DataPacket));

	if(setup != NULL) {
		setupPacket(&dataPacket,setup,endpoint);
	} else {
		dataPacket.length = 64;
	}

	dataPacket.endpoint = endpoint;

	write(newsockfd,(unsigned char*)&dataPacket,sizeof(struct DataPacket));

	unsigned char* changedBuff = (unsigned char*)malloc(length+2);
	memset(changedBuff,0x00,length+2);	
	memcpy(changedBuff,(unsigned char*)&length,2);

	memcpy(&changedBuff[2],buff,length);

	int writeVal = write(newsockfd,changedBuff,length+2);

	free(changedBuff);

	pthread_mutex_unlock(&deviceMutex);

	return writeVal;
}

int receiveTransaction(struct usb_ctrlrequest *setup, uint8_t endpoint,unsigned char* buff,uint16_t length) {

	pthread_mutex_lock(&deviceMutex);

	struct DataPacket dataPacket;

	memset(&dataPacket,0x00,sizeof(struct DataPacket));

	if(setup != NULL) {
		setupPacket(&dataPacket,setup,endpoint);
	} else {
		dataPacket.length = 64;
	}

	dataPacket.endpoint = endpoint;

	// printf("Writing new sock datapacket: %02x\n",endpoint);
	
	unsigned char* changedBuff = (unsigned char*)malloc(length+2);
	memset(changedBuff,0x00,length+2);	

	int writeDataPacketRet = write(newsockfd,(unsigned char*)&dataPacket,sizeof(struct DataPacket));

	// printf("Reading data back\n");

	int readVal = read(newsockfd,changedBuff,length+2);

	uint16_t receivedVal;
	memcpy(&receivedVal,changedBuff,2);

	memcpy(buff,&changedBuff[2],receivedVal);

	printf("Got readval: (%d) ",receivedVal);

	for(int i = 0 ; i < receivedVal ; i++) {
		printf("%02x ",buff[i]);
	}
	printf("\n");

	free(changedBuff);

	pthread_mutex_unlock(&deviceMutex);

	// printf("Exiting receivedVal\n");

	return receivedVal;
}

static void handleSetup(struct usb_ctrlrequest *setup) {
	
	uint16_t value = __le16_to_cpu(setup->wValue);
	uint16_t index = __le16_to_cpu(setup->wIndex);
	uint16_t length = __le16_to_cpu(setup->wLength);

	// printf("Got USB bRequest: %d(%02x) with type %d(%02x) of length %d\n",setup->bRequest,setup->bRequest,setup->bRequestType, setup->bRequestType, setup->wLength);

	// start transactions

	unsigned char* buf = (unsigned char*)malloc(length);

	if(setup->bRequestType&0x80) {

		/* get buff */
		// printf("Receiving from host\n");
		receiveTransaction(setup,0x00,buf,length);
		write(gadgetFile, buf, length);

	} else {

		/* send buff */
		// printf("Sending to host\n");
		read(gadgetFile, buf, length);
		sendTransaction(setup,0x00,buf,length);

	}

	// printf("Got buff: ");
	// for(int i = 0 ; i < length ; i++) {
	// 	printf("%02x ",buf[i]);
	// }
	// printf("\n");

	free(buf);

}

struct EndpointInfo {
	uint8_t ep;
	unsigned char buff[64];
	uint8_t epMaxPacketSize;
};

struct EndpointInfo endpointInfo[32];

int pollEpsInc;
struct pollfd pollEps[32];
uint32_t setEps = 0;

static void* checkEps(void* nothing) {

	int i = *((int*)nothing);

	printf("Nothing value: %d\n",i);

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

	while(true) {

		int pollVal = poll((pollfd*)&pollEps,pollEpsInc,5);

		// for(int i = 0 ; i < pollEpsInc ; i++) {

			// printf("Checking pollval on: %d\n",i);

			if((pollVal >= 0 || endpointInfo[i].ep&0x80) && setEps&((1<<(endpointInfo[i].ep&0xf))<<((endpointInfo[i].ep&0x80)?16:0))) { 

				// printf("Checking revents on: %d\n",i);

				if(pollEps[i].revents || endpointInfo[i].ep&0x80) {

					int readCount = 0;

					if(endpointInfo[i].ep&0x80) {

						// do host request
						// printf("Before endpoint in transaction\n");
						readCount = receiveTransaction(NULL,endpointInfo[i].ep,endpointInfo[i].buff,64);

						printf("(%d)Requesting in (%d): ",pollEps[i].fd,readCount);
						if(readCount > 0) {
							int writeVal = write(pollEps[i].fd, endpointInfo[i].buff, readCount);
						}

					} else {

						// printf("On requesting out\n");

						readCount = read(pollEps[i].fd,endpointInfo[i].buff,64);
						printf("(%d)Requesting out (%d): ",pollEps[i].fd,readCount);

						int readLength = sendTransaction(NULL,endpointInfo[i].ep,endpointInfo[i].buff,64);

						// printf("After send transaction\n");

					}

					for(int j = 0 ; j < readCount ; j++) {
						printf("%02x ",endpointInfo[i].buff[j]);
					}
					printf("\n");

				}
			}		

		// }

	}

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

	while(true) {
		
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
	shutdown(newsockfd,SHUT_RDWR);
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

	int enable = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
    	printf("setsockopt(SO_REUSEADDR) failed\n");
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

		// sleep(1);

		int epPtr = 4;

		setEps = 0;
		pollEpsInc = 0;

		while(epPtr < gadgetFileSize) {

			int readSize = buffer[epPtr];

			if(buffer[epPtr+1] == 5 && (setEps&(1<<(buffer[epPtr+2]&0xf))<<((buffer[epPtr+2]&0x80)?16:0))==0) { // check if hid descriptor

				printf("Found endpoint: %02x\n",buffer[epPtr+2]);

				char epBuffStr[0xff];
				sprintf(epBuffStr,"/dev/gadget/ep%d%s",buffer[epPtr+2]&0xf,(buffer[epPtr+2]&0x80)?"in":"out");

				int file = open(epBuffStr, O_CLOEXEC | O_RDWR | O_NONBLOCK);
				printf("Writing to file %s (%d)\n",epBuffStr,file);

				if(file>=0) {

					unsigned char* epFileBuff = (unsigned char*)malloc(4+readSize*2);

					memset(epFileBuff,0x00,4+readSize*2);
					epFileBuff[0] = 0x01;
					int epFileBuffInc = 4;
					memcpy(&epFileBuff[epFileBuffInc],&buffer[epPtr],readSize);
					epFileBuffInc += readSize;
					memcpy(&epFileBuff[epFileBuffInc],&buffer[epPtr],readSize);

					printf("Writing to %s (%d): ",epBuffStr,4+readSize*2);
					for(int i = 0 ; i < (4+readSize*2) ; i++) {
						printf("%02x ",epFileBuff[i]);
					}
					printf("\n");

					int epWrite = -1;

					while(epWrite < 0) {
				    	epWrite = write(file,epFileBuff,4+readSize*2);
					}
				    free(epFileBuff);
					// printf("Ep write got: %d\n",epWrite);

					pollEps[pollEpsInc].fd = file;

					pollEps[pollEpsInc].events=POLLIN | POLLOUT;

					endpointInfo[pollEpsInc].ep = buffer[epPtr+2];

					int* arg = (int*)malloc(4);
					*arg = pollEpsInc;

					pthread_create(&epThreads[pollEpsInc],0,checkEps,(void*)arg);

					pollEpsInc++;
    				
				}

				setEps |= (1<<(buffer[epPtr+2]&0xf))<<((buffer[epPtr+2]&0x80)?16:0);
			}
			epPtr += readSize;
		}


	}

	printf("Exiting\n");

	close(newsockfd);
	close(sockfd);
}
