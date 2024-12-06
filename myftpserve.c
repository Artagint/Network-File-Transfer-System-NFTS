#include "myftp.h"

int serverSide(int portNumber, int debug);
void serverCommands(int connectfd, int debug);

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

void serverCommands(int connectfd, int debug){
	char buffer[1024];

	while(1){
		int readChar;
		int i = 0;
		while(i < sizeof(buffer) - 1){
			readChar = read(connectfd, &buffer[i], 1);
			if(buffer[i] == '\n') break;
			i++;
		}	
		buffer[i] = '\0';
		if(debug) printf("DEBUG: Child '%d' sent: '%s'\n", getpid(), buffer);

		if(strcmp(buffer, "Q") == 0){
			const char *serverResponse = "A\n";
			if(write(connectfd, serverResponse, strlen(serverResponse)) < 0){
				fprintf(stderr, "ERROR: Can't write 'A' back to client\n");
			}
			break;
		}
		else if(buffer[0] == 'C'){
			if(debug) printf("DEBUG: 'rcd' command received: '%s'\n", (buffer + 1));
			if(chdir(buffer + 1) == 0){
				const char *serverResponse = "A\n";
				if(write(connectfd, serverResponse, strlen(serverResponse)) < 0){
					fprintf(stderr, "ERROR: Child %d can't send 'A'\n", getpid());
				}
				printf("Child %d: Changed current directory to '%s'\n", getpid(), (buffer + 1));
			}
			else{
				char serverResponse[1024];
				snprintf(serverResponse, sizeof(serverResponse), "E%s\n", strerror(errno));
				if(write(connectfd, serverResponse, strlen(serverResponse)) < 0){
					fprintf(stderr, "ERROR: Child %d can't send 'E'\n", getpid());
				}
				else if(debug) printf("DEBUG: Child %d: Sending acknowledgment -> %s", getpid(), serverResponse);
			}
		}
		else if(strcmp(buffer, "L") == 0){
			if(debug) printf("DEBUG: 'rls' command received: '%s'\n", buffer);
		}
		else if(buffer[0] == 'G'){
			if(debug) printf("DEBUG: command received: '%s'\n", (buffer + 1));
		}
		else if(buffer[0] == 'P'){
			if(debug) printf("DEBUG: 'put' command received: '%s'\n", (buffer + 1));
		}
		else if(strcmp(buffer, "D") == 0){
			if(debug) printf("DEBUG: establishing data connection\n");
		}
		else{
			printf("ERROR: Unknown command: '%s'\n", buffer);
			const char *writeError = "EUnknown command\n";
			if(write(connectfd, writeError, strlen(writeError)) < 0){
				fprintf(stderr, "ERROR: Can't write 'E' back to client\n");
			}
		}
	}
	close(connectfd);
	printf("Client session ended\n");
}

int serverSide(int portNumber, int debug){
	int listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if(listenfd < 0){
		fprintf(stderr, "Error: %s\n", strerror(errno));
		return errno;
	}
	if(debug) printf("DEBUG: Parent: socket made with descriptor '%d'\n", listenfd);

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
	if(debug) printf("DEBUG: Parent: socket bound to port '%d'\n", portNumber);

	if(listen(listenfd, 4) < 0){
		fprintf(stderr, "Error: %s\n", strerror(errno));
		close(listenfd);
		return errno;
	}

	while(1){
		int connectfd;
		int length = sizeof(struct sockaddr_in);
		struct sockaddr_in clientAddr;

		connectfd = accept(listenfd, (struct sockaddr *) &clientAddr, &length);
		if(connectfd < 0){
			fprintf(stderr, "Error: %s\n", strerror(errno));
			continue;
		}

		pid_t pid = fork();
		if(pid == 0){
			close(listenfd);
			if(debug) printf("DEBUG: Child %d started\n", getpid());
			serverCommands(connectfd, debug);
			if(debug) printf("DEBUG: Child %d quitting\n", getpid());
			close(connectfd);
			exit(0);
		}
		else{
			close(connectfd);
		}
	}
	close(listenfd);
	return 0;
}
