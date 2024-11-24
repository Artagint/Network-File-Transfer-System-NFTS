#include "myftp.h"

int serverSide(int portNumber, int debug);

int main(int argc, char const *argv[]){
	int debug = 0;
	int portNumber = 49999;

	for(int i = 1; i < argc; i++){
		if(strcmp(argv[i], "-d") == 0){
			debug = 1;		
		}
		else if(strcmp(argv[i], "-p") == 0){
			if(i + 1 < argc){
				i++;
				portNumber = atoi(argv[i]);
				if(portNumber < 1024 || portNumber > 65535){
					fprintf(stderr, "Error: Port number must be between 1024 and 65535\n");
					return 1;
				}
			}
			else{
				fprintf(stderr, "Error: <-p> should be followed by a port number\n");
				return 1;
			}
		}
		else{
			fprintf(stderr, "Error: format should be <./myftpserve -d -p port_number>\n");
			fprintf(stderr, "Where <-d> and <-p port_number> are optional and <-p> must be followed by <port_number>\n");
			return 1;
		}
	}
	return serverSide(portNumber, debug);
}

int serverSide(int portNumber, int debug){
	int listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if(listenfd < 0){
		fprintf(stderr, "Error: %s\n", strerror(errno));
		return errno;
	}

	if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0){
		fprintf(stderr, "Error: %s\n", strerror(errno));
		close(listenfd);
		return errno;
	}

	struct sockaddr_in servAddr;
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(portNumber);
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(listenfd, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0){
		fprintf(stderr, "Error: %s\n", strerror(errno));
		close(listenfd);
		return errno;
	}

	if(listen(listenfd, 4) < 0){
		fprintf(stderr, "Error: %s\n", strerror(errno));
		close(listenfd);
		return errno;
	}
	printf("Server listening on port %d...\n", portNumber);

	while(1){
		int connectfd;
		int length = sizeof(struct sockaddr_in);
		struct sockaddr_in clientAddr;

		connectfd = accept(listenfd, (struct sockaddr *) &clientAddr, &length);
		if(connectfd < 0){
			fprintf(stderr, "Error: %s\n", strerror(errno));
			continue;
		}

		if(fork()){
			close(connectfd);
		}
		else{
			close(listenfd);

			char msg[] = "Hello, World!\n";
			if(write(connectfd, msg, strlen(msg)) < 0){
				fprintf(stderr, "Error: %s\n", strerror(errno));
			}
			close(connectfd);
			exit(0);
		}
	}
	close(listenfd);
	return 0;
}
