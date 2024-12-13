#include "myftp.h"

int serverSide(int portNumber, int debug);
void serverCommands(int connectfd, int debug);
int serverDataConnection(int connectfd, int debug);
int readResponse(int socketfd, char *buffer, int bufferSize);
void Qcommand(int connectfd);
void Ccommand(int connectfd, const char *pathName, int debug);
void Lcommand(int connectfd, int dataSocket, int debug);
void Gcommand(int connectfd, int dataSocket, const char *fileBuffer, int debug);
void Pcommand(int connectfd, int dataSocket, const char *fileBuffer, int debug);

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
	char clientResponse[PATH_MAX + 2];

	while(1){
		if(readResponse(connectfd, clientResponse, sizeof(clientResponse)) != 0){
			fprintf(stderr, "ERROR: Child %d: Fatal error, exiting\n", getpid());
			break;
		}
		if(debug) printf("DEBUG: Child '%d' sent: '%s'\n", getpid(), clientResponse);

		if(strcmp(clientResponse, "Q") == 0){
			Qcommand(connectfd);
			break;
		}
		else if(clientResponse[0] == 'C'){
			Ccommand(connectfd, (clientResponse + 1), debug);
		}
		else if(strcmp(clientResponse, "D") == 0){
			if(debug) printf("DEBUG: establishing data connection\n");
			int dataSocket = serverDataConnection(connectfd, debug);
			if(dataSocket < 0){
				fprintf(stderr, "ERROR: Failed to establish data connection\n");
			}
			else{
				if(debug) printf("DEBUG: Data connection established\n");
				char commandBuffer[PATH_MAX + 2];
				if(readResponse(connectfd, commandBuffer, sizeof(commandBuffer)) != 0){
					fprintf(stderr, "ERROR: Can't read command after 'D'\n");
					close(dataSocket);
					continue;
				}
				if(debug) printf("DEBUG: '%s' received after 'D'\n", commandBuffer);

				if(strcmp(commandBuffer, "L") == 0){
					Lcommand(connectfd, dataSocket,  debug);
				}
				else if(commandBuffer[0] == 'G'){
					char *fileBuffer = (commandBuffer + 1);
					Gcommand(connectfd, dataSocket, fileBuffer, debug);
				}
				else if(commandBuffer[0] == 'P'){
					char *fileBuffer = (commandBuffer + 1);
					Pcommand(connectfd, dataSocket, fileBuffer, debug);					 
				}
				else{
					fprintf(stderr, "ERROR: Unknown command after 'D'. Has to be 'L', 'G', or 'P'\n");
					const char *errResponse = "EUnknown Command\n";
					write(connectfd, errResponse, strlen(errResponse));
				}
				close(dataSocket);
				if(debug) printf("DEBUG: Data socket closed\n");
			}
		}
		else{
			printf("ERROR: Unknown command: '%s'\n", clientResponse);
			const char *writeError = "EUnknown command\n";
			if(write(connectfd, writeError, strlen(writeError)) < 0){
				fprintf(stderr, "ERROR: Can't write 'E' back to client\n");
			}
		}
	}
	close(connectfd);
	printf("Child %d: Client session ended\n", getpid());
}

int readResponse(int socketfd, char *buffer, int bufferSize){
        int i = 0;
        while(1){
                int rsp = read(socketfd, &buffer[i], 1);
                if(rsp < 0){
                        fprintf(stderr, "ERROR: Failed to read: '%s'\n", strerror(errno));
                        return errno;
                }
                if(rsp == 0){
                        fprintf(stderr, "ERROR: Child %d: Control socket EOF detected, exiting\n", getpid());
                        return errno;
                }
                if(buffer[i] == '\n') break;
                i++;
        }
        buffer[i] = '\0';
        return 0;
}

int serverDataConnection(int connectfd, int debug){
	int listenfd = socket(AF_INET, SOCK_STREAM, 0);
	char errResponse[1024];
	if(listenfd < 0){
		fprintf(stderr, "ERROR: Can't make data socket: '%s'\n", strerror(errno));
		snprintf(errResponse, sizeof(errResponse), "ECan't make data socket\n");
		write(connectfd, errResponse, strlen(errResponse));
		return 1;
	}
	if(debug) printf("DEBUG: Succesfully made data socket\n");

	struct sockaddr_in dataAddr;
	memset(&dataAddr, 0, sizeof(dataAddr));
	dataAddr.sin_family = AF_INET;
	dataAddr.sin_port = 0;
	dataAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(listenfd, (struct sockaddr *)&dataAddr, sizeof(dataAddr)) < 0){
		fprintf(stderr, "ERROR: Can't bind data socket: '%s'\n", strerror(errno));
		close(listenfd);
		snprintf(errResponse, sizeof(errResponse), "ECan't bind data socket\n");
		write(connectfd, errResponse, strlen(errResponse));
		return 1;
	}
	if(debug) printf("DEBUG: Succesfully binded data socket\n");

	int length = sizeof(dataAddr);
	if(getsockname(listenfd, (struct sockaddr *)&dataAddr, &length) < 0){
		fprintf(stderr, "ERROR: Can't get socket name: '%s'\n", strerror(errno));
		close(listenfd);
		snprintf(errResponse, sizeof(errResponse), "ECan't get socket name\n");
		write(connectfd, errResponse, strlen(errResponse));
		return 1;
	}
	int assignedPortNum = ntohs(dataAddr.sin_port);
	if(debug) printf("DEBUG: Epheramal port number assigned: '%d'\n", assignedPortNum);

	if(listen(listenfd, 1) < 0){
		fprintf(stderr, "ERROR: Can't listen on data socket: '%s'\n", strerror(errno));
		close(listenfd);
		snprintf(errResponse, sizeof(errResponse), "ECan't listen on data socket\n");
		write(connectfd, errResponse, strlen(errResponse));
		return 1;
	}
	if(debug) printf("DEBUG: Listening on data socket\n");

	char resToClient[16];
	snprintf(resToClient, sizeof(resToClient), "A%d\n", assignedPortNum);
	if(write(connectfd, resToClient, strlen(resToClient)) < 0){
		fprintf(stderr, "ERROR: Can't write port number to client: '%s'\n", strerror(errno));
		close(listenfd);
		return 1;
	}
	if(debug) printf("DEBUG: Data connection made on port '%d'\n", assignedPortNum);

	int clientDataSock = accept(listenfd, NULL, NULL);
	if(clientDataSock < 0){
		fprintf(stderr, "ERROR: Can't accept the data connection: '%s'\n", strerror(errno));
		close(listenfd);
		return 1;
	}
	if(debug) printf("DEBUG: Data connection with client established on port: '%d'\n", assignedPortNum);

	close(listenfd);
	return clientDataSock;
}

void Qcommand(int connectfd){
        const char *serverResponse = "A\n";
        if(write(connectfd, serverResponse, strlen(serverResponse)) < 0){
                fprintf(stderr, "ERROR: Can't write 'A' back to client\n");
        }
}

void Ccommand(int connectfd, const char *pathName, int debug){
        if(debug) printf("DEBUG: 'rcd' command received: '%s'\n", pathName);

        if(chdir(pathName) == 0) {
                const char *serverResponse = "A\n";
                if(write(connectfd, serverResponse, strlen(serverResponse)) < 0) {
                        fprintf(stderr, "ERROR: Child %d can't send 'A'\n", getpid());
                }
                printf("Child %d: Changed current directory to '%s'\n", getpid(), pathName);
        } else {
                char serverResponse[1024];
                snprintf(serverResponse, sizeof(serverResponse), "E%s\n", strerror(errno));
                if(write(connectfd, serverResponse, strlen(serverResponse)) < 0) {
                        fprintf(stderr, "ERROR: Child %d can't send 'E'\n", getpid());
                }
		printf("Child %d: cd to %s failed with error E%s\n", getpid(), pathName, strerror(errno));
                if(debug) printf("DEBUG: Child %d: Sending acknowledgment -> %s", getpid(), serverResponse);
        }
}

void Lcommand(int connectfd, int dataSocket, int debug){
        if(debug) printf("DEBUG: 'rls' command received\n");

        int fd[2];
        if(pipe(fd) < 0){
		fprintf(stderr, "ERROR: Can't make pipe: '%s'\n", strerror(errno));
		close(dataSocket);
		return;
	}

        pid_t lsFork = fork();
        if(lsFork < 0){
                fprintf(stderr, "ERROR: Fork for ls failed '%s'\n", strerror(errno));
                close(dataSocket);
                return;
        }
        if(lsFork == 0){
                close(fd[0]);
                dup2(fd[1], 1);
                close(fd[1]);
                execlp("ls", "ls", "-l", NULL);
                fprintf(stderr, "ERROR: Failed to execute 'ls': '%s'\n", strerror(errno));
                exit(1);
        }
        close(fd[1]);

        char buffer[PATH_MAX + 2];
        int numOfBytesRead;
        while((numOfBytesRead = read(fd[0], buffer, sizeof(buffer))) > 0){
                if(write(dataSocket, buffer, numOfBytesRead) < 0){
                        fprintf(stderr, "ERROR: Can't write to socket: '%s'\n", strerror(errno));
                        return;
                }
        }
        close(fd[0]);
        close(dataSocket);
        wait(NULL);

        const char *serverResponse = "A\n";
        if(write(connectfd, serverResponse, strlen(serverResponse)) < 0){
                fprintf(stderr, "ERROR: Can't send 'A' to client: '%s'\n", strerror(errno));
		return;
        }
        if(debug) printf("DEBUG: 'rls' command complete\n");
}

void Gcommand(int connectfd, int dataSocket, const char *fileBuffer, int debug){
	if(debug) printf("DEBUG: File path: '%s'\n", fileBuffer);

	printf("Child %d: Reading file '%s'\n", getpid(), fileBuffer);

	int fd = open(fileBuffer, O_RDONLY);
	if(fd < 0){
		char errResponse[PATH_MAX + 2];
		snprintf(errResponse, sizeof(errResponse), "E%s\n", strerror(errno));
		write(connectfd, errResponse, strlen(errResponse));
		if(debug) printf("DEBUG: Can't open file '%s': '%s'\n", fileBuffer, strerror(errno));
		return;
	}

	const char *serverResponse = "A\n";
	if(write(connectfd, serverResponse, strlen(serverResponse)) < 0){
		fprintf(stderr, "ERROR: Can't send 'A' to client\n");
		close(fd);
		return;
	}
	if(debug) printf("DEBUG: Sent 'A' to client\n");\

	printf("Child %d: Transmitting file '%s' to client\n", getpid(), fileBuffer);

	if(debug) printf("DEBUG: Transferring '%s' to client\n", fileBuffer);
	char buffer[4096];
	int numOfBytesRead;
	while((numOfBytesRead = read(fd, buffer, sizeof(buffer))) > 0){
		if(write(dataSocket, buffer, numOfBytesRead) < 0){
			fprintf(stderr, "ERROR: Can't write to client: '%s'\n", strerror(errno));
			close(fd);
			close(dataSocket);
			return;
		}
	}
	if(numOfBytesRead < 0){
		fprintf(stderr, "ERROR: Cant' read file: '%s'\n", strerror(errno));
	}
	close(fd);
	close(dataSocket);
}

void Pcommand(int connectfd, int dataSocket, const char *fileBuffer, int debug){
	if(debug) printf("DEBUG: 'put' command received, file '%s'\n", fileBuffer);

	printf("Child %d: Writing file %s\n", getpid(), fileBuffer);

	int fd = open(fileBuffer, O_CREAT | O_WRONLY | O_EXCL, 0777);
	if(fd < 0){
		char errResponse[PATH_MAX + 2];
		snprintf(errResponse, sizeof(errResponse), "E%s\n", strerror(errno));
		write(connectfd, errResponse, strlen(errResponse));
		if(debug) printf("DEBUG: Can't open file '%s': '%s'\n", fileBuffer, strerror(errno));
		return;
	}

	const char *serverResponse = "A\n";
	if(write(connectfd, serverResponse, strlen(serverResponse)) < 0){
		fprintf(stderr, "ERRROR: Can't send 'A' to client\n");
		close(fd);
		return;
	}
	if(debug) printf("DEBUG: Sent 'A' to client\n");

	printf("Child %d: Receiving file '%s' from client\n", getpid(), fileBuffer);

	char buffer[4096];
	int numOfBytesRead;
	while((numOfBytesRead = read(dataSocket, buffer, sizeof(buffer))) > 0){
		if(write(fd, buffer, numOfBytesRead) < 0){
			fprintf(stderr, "ERROR: Can't write to file '%s'\n", strerror(errno));
			close(fd);
			close(dataSocket);
			return;
		}
	}
	if(debug) printf("DEBUG: Wrote '%d' byte to file\n", numOfBytesRead);

	if(numOfBytesRead < 0){
		fprintf(stderr, "ERROR: Cant read from client: '%s'\n", strerror(errno));
	}
	close(fd);
	close(dataSocket);
	if(debug) printf("DEBUG: Data connection closed\n");
}

int serverSide(int portNumber, int debug){
	int listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if(listenfd < 0){
		fprintf(stderr, "ERROR: Can't amke socket: %s\n", strerror(errno));
		return errno;
	}
	if(debug) printf("DEBUG: Parent: socket made with descriptor '%d'\n", listenfd);

	if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0){
		fprintf(stderr, "ERROR: Can't set socket options: %s\n", strerror(errno));
		close(listenfd);
		return errno;
	}

	struct sockaddr_in servAddr;
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(portNumber);
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(listenfd, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0){
		fprintf(stderr, "ERROR: Can't bind socket to port '%d': %s\n", portNumber, strerror(errno));
		close(listenfd);
		return errno;
	}
	if(debug) printf("DEBUG: Parent: socket bound to port '%d'\n", portNumber);

	if(listen(listenfd, 4) < 0){
		fprintf(stderr, "ERROR: Can't listen on socket, port '%d': %s\n", portNumber, strerror(errno));
		close(listenfd);
		return errno;
	}

	while(1){
		int connectfd;
		int length = sizeof(struct sockaddr_in);
		struct sockaddr_in clientAddr;

		while(waitpid(-1, NULL, WNOHANG) > 0);

		connectfd = accept(listenfd, (struct sockaddr *) &clientAddr, &length);
		if(connectfd < 0){
			fprintf(stderr, "ERROR: Can't accept client connection: %s\n", strerror(errno));
			continue;
		}

		pid_t pid = fork();
		if(pid < 0){
			fprintf(stderr, "ERROR: Can't fork process: '%s'\n", strerror(errno));
			close(connectfd);
		}
		if(pid == 0){
			close(listenfd);
			printf("Child %d: Connection accepted\n", getpid());
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
