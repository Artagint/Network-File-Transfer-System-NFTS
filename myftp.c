#include "myftp.h"

int clientSide(const char *address, const char *somePort, int debug);
void clientCommands(int debug, int socketfd);

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

void clientCommands(int debug, int socketfd){
	char userInput[1024];
	char *userCommand;
	char *pathName;

	while(1){
		printf("MFTP> ");
		fflush(stdout);

		if(fgets(userInput, sizeof(userInput), stdin) == NULL){
			fprintf(stderr, "ERROR: Input can't be read, program exited\n");
			break;
		}
		userInput[strcspn(userInput, "\n")] = '\0';

		userCommand = strtok(userInput, " ");
		if(userCommand == NULL) continue;
		pathName = strtok(NULL, "");

		if(debug){
			printf("DEBUG: user command '%s' with pathname '%s'\n", userCommand, pathName);
		}


		if(strcmp(userCommand, "exit") == 0){
			char serverResponse[1024];
			if(write(socketfd, "Q\n", 2) < 0){
				fprintf(stderr, "ERROR: Can't send command to server\n");
				return;
			}
			
			int numOfBytes = read(socketfd, serverResponse, sizeof(serverResponse) - 1);
			if(numOfBytes > 0){
				serverResponse[strcspn(serverResponse, "\n")] = '\0';
				if(debug) printf("DEBUG: Server reponse: '%s'\n", serverResponse);

				if(serverResponse[0] == 'A'){
				       	printf("Client exiting normally\n");
				}

			}
			else{
				fprintf(stderr, "ERROR: Server response not read\n");
			}
			close(socketfd);
			return;
		}
		else if(strcmp(userCommand, "cd") == 0){
			if(pathName){
				if(chdir(pathName) == 0){
					printf("Changed local directory to '%s\n", pathName);
				}
				else{
					fprintf(stderr, "ERROR: Change directory: '%s'\n", strerror(errno));
				}
			}
			else{
				fprintf(stderr, "ERROR: proper format is 'cd <pathname>'\n");
			}
		}
		else if(strcmp(userCommand, "rcd") == 0){
			if(pathName){
				char path[1024];
				char serverResponse[1024];
				snprintf(path, sizeof(path), "C%s\n", pathName);

				if(write(socketfd, path, strlen(path)) < 0){
					fprintf(stderr, "ERROR: Can't send command to server\n");
					return;
				}

				int numOfBytes = read(socketfd, serverResponse, sizeof(serverResponse) - 1);
				if(numOfBytes > 0){
					serverResponse[strcspn(serverResponse, "\n")] = '\0';
					if(debug) printf("DEBUG: Server response: '%s'\n", serverResponse);

					if(serverResponse[0] == 'A'){
						printf("Changed remote directory to '%s'\n", pathName);
					}
					else if(serverResponse[0] == 'E'){
						fprintf(stderr, "ERROR: response from server: '%s'\n", (serverResponse + 1));
					}
				}
			}
			else{
				fprintf(stderr, "ERROR: proper format is 'rcd <pathname>'\n");
			}
		}
		else if(strcmp(userCommand, "ls") == 0){
			printf("%s\n", userCommand);
		}
		else if(strcmp(userCommand, "rls") == 0){
			printf("%s\n", userCommand);
		}
		else if(strcmp(userCommand, "get") == 0){
			if(pathName){
				printf("%s %s\n", userCommand, pathName);
			}
			else{
				fprintf(stderr, "ERROR: proper format is 'get <pathname>'\n");
			}
		}
		else if(strcmp(userCommand, "show") == 0){
			if(pathName){
				printf("%s %s\n", userCommand, pathName);
			}
			else{
				fprintf(stderr, "ERROR: proper format is 'show <pathname>'\n");
			}
		}
		else if(strcmp(userCommand, "put") == 0){
			if(pathName){
				printf("%s %s\n", userCommand, pathName);
			}
			else{
				fprintf(stderr, "ERROR: proper format is 'put <pathname>'\n");
			}
		}
		else{
			fprintf(stderr, "ERROR: Command '%s' is invalid\n", userCommand);
		}


	}

}

int clientSide(const char *address, const char *somePort, int debug){
	int socketfd;
	struct addrinfo hints, *actualdata;
	memset(&hints, 0, sizeof(hints));
	int err;

	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_INET;
	err = getaddrinfo(address, somePort, &hints, &actualdata);
	if(err != 0){
		fprintf(stderr, "Error: %s\n", gai_strerror(err));
		return 1;
	}

	socketfd = socket(actualdata -> ai_family, actualdata -> ai_socktype, 0);
	if(socketfd < 0){
		fprintf(stderr, "Error: %s\n", strerror(errno));
		freeaddrinfo(actualdata);
		return errno;
	}
	if(debug) printf("DEBUG: Socket made with descriptor '%d'\n", socketfd);

	if(connect(socketfd, actualdata -> ai_addr, actualdata -> ai_addrlen) < 0){
		fprintf(stderr, "Error: %s\n", strerror(errno));
		close(socketfd);
		freeaddrinfo(actualdata);
		return errno;
	}
	if(debug) printf("DEBUG: Connected to server with address: %s on port: %s\n", address, somePort);

	clientCommands(debug, socketfd);

	close(socketfd);
	freeaddrinfo(actualdata);
	return 0;
}
