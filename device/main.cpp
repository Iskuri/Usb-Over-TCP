#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include "../inc/defines.h"

bool run = true;

int sockfd, newsockfd, portno;

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

		int ret = read(newsockfd,buffer,65536);
		printf("Read %d\n",ret);

	}

	printf("Exiting\n");

	close(newsockfd);
	close(sockfd);
}
