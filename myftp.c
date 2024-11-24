#include "myftp.h"

int clientSide(const char *address, const char *somePort, int debug);

int main(int argc, char const *argv[]){
	int debug = 0;
	if(strcmp(argv[1], "-d") == 0) debug = 1;

	const char *somePort;
	const char *address;
	if(debug){
		somePort = argv[2];
		address = argv[3];
	}
	else{
		somePort = argv[1];
		address = argv[2];
	}

	return clientSide(address, somePort, debug);
}

int clientSide(const char *address, const char *somePort, int debug){
	int socketfd;
	struct addrinfo hints, *actualdata;
	memset(&hints, 0, sizeof(hints));
	int err;
	if(debug) printf("DEBUG: Server address %s on port %s\n", address, somePort);

	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_INET;
	err = getaddrinfo(address, somePort, &hints, &actualdata);
	if(err != 0){
		fprintf(stderr, "Error: %s\n", gai_strerror(err));
		return 1;
	}
	if(debug) printf("DEBUG: Server address and port resolved\n");

	socketfd = socket(actualdata -> ai_family, actualdata -> ai_socktype, 0);
	if(socketfd < 0){
		fprintf(stderr, "Error: %s\n", strerror(errno));
		freeaddrinfo(actualdata);
		return errno;
	}
	if(debug) printf("DEBUG: Socket made\n");

	if(connect(socketfd, actualdata -> ai_addr, actualdata -> ai_addrlen) < 0){
		fprintf(stderr, "Error: %s\n", strerror(errno));
		close(socketfd);
		freeaddrinfo(actualdata);
		return errno;
	}
	if(debug) printf("DEBUG: Connected to server with address: %s on port: %s\n", address, somePort);

	char buffer[502];
	int bytesRead = read(socketfd, buffer, sizeof(buffer) - 1);
	if(bytesRead < 0){
		fprintf(stderr, "Error: %s\n", strerror(errno));
		close(socketfd);
		freeaddrinfo(actualdata);
		return errno;
	}
	buffer[bytesRead] = '\0';
	printf("Message from server: %s", buffer);

	close(socketfd);
	freeaddrinfo(actualdata);
	return 0;
}
