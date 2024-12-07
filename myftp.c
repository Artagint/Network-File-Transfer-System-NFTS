#include "myftp.h"

int clientSide(const char *address, const char *somePort, int debug);
void clientCommands(int debug, int socketfd);
int establishDataConnection(int socket, int debug);
int readResponse(int socketfd, char *buffer, int bufferSize);

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
			char buffer[PATH_MAX + 2];
			if(write(socketfd, "Q\n", 2) < 0){
				fprintf(stderr, "ERROR: Can't send command to server\n");
			}
			if(readResponse(socketfd, buffer, sizeof(buffer)) != 0) break;
			if(debug) printf("DEBUG: Server reponse: '%s'\n", buffer);

			if(buffer[0] == 'A'){
			       	printf("Client exiting normally\n");	
			}
			else{
				fprintf(stderr, "ERROR: Server response not read\n");
			}
			close(socketfd);
			break;
		}
		else if(strcmp(userCommand, "cd") == 0){
			if(pathName){
				if(chdir(pathName) == 0){
					printf("Changed local directory to '%s\n", pathName);
				}
				else{
					fprintf(stderr, "ERROR: Can't change directory: '%s'\n", strerror(errno));
					continue;
				}
			}
			else{
				fprintf(stderr, "ERROR: proper format is 'cd <pathname>'\n");
				continue;
			}
		}
		else if(strcmp(userCommand, "rcd") == 0){
			if(pathName){
				char path[1024];
				char buffer[PATH_MAX + 2];
				snprintf(path, sizeof(path), "C%s\n", pathName);

				if(write(socketfd, path, strlen(path)) < 0){
					fprintf(stderr, "ERROR: Can't send command to server\n");
					continue;
				}
				if(readResponse(socketfd, buffer, sizeof(buffer)) != 0) continue;
				if(debug) printf("DEBUG: Server response: '%s'\n", buffer);

				if(buffer[0] == 'A'){
					printf("Changed remote directory to '%s'\n", pathName);
				}
				else if(buffer[0] == 'E'){
					fprintf(stderr, "ERROR: response from server: '%s'\n", (buffer + 1));
					continue;
				}
			}
			else{
				fprintf(stderr, "ERROR: proper format is 'rcd <pathname>'\n");
				continue;
			}
		}
		else if(strcmp(userCommand, "ls") == 0){
			int fd[2];
			pipe(fd);

			pid_t lsFork = fork();
			if(lsFork < 0){
				fprintf(stderr, "ERRROR: fork for ls failed\n");
				continue;
			}
			if(lsFork == 0){
				close(fd[0]);
				dup2(fd[1], 1);
				close(fd[1]);
				if(debug) printf("DEBUG: Child process '%d' starting 'ls'\n", getpid());
				execlp("ls", "ls", "-l", NULL);
				fprintf(stderr, "ERROR: ls failed to execute\n");
				exit(1);
			}

			pid_t moreFork = fork();
			if(moreFork < 0){
				fprintf(stderr, "ERROR: fork for more failed\n");
				continue;
			}
			if(moreFork == 0){
				close(fd[1]);
				dup2(fd[0], 0);
				close(fd[0]);
				if(debug) printf("DEBUG: Child process '%d' starting 'more'\n", getpid());
				execlp("more", "more", "-20", NULL);
				fprintf(stderr, "ERROR more failed to execute\n");
				exit(1);
			}
			close(fd[0]);
			close(fd[1]);
			wait(NULL);
			wait(NULL);
			if(debug) printf("DEBUG: ls execution complete\n");	
		}
		else if(strcmp(userCommand, "rls") == 0){
			char serverResponse[1024];
			if(write(socketfd, "L\n", 2) < 0){
                                fprintf(stderr, "ERROR: Can't send command to server\n");
                                return;
                        }
			if(debug) printf("DEBUG: sent rls command to server\n");
		}
		else if(strcmp(userCommand, "get") == 0){
			if(pathName){
                    		int dataSocket = establishDataConnection(socketfd, debug);
				if(dataSocket < 0){
					fprintf(stderr, "ERROR: Failed to establish data connection\n");
					return;
				}

				char path[1024];
                                char buffer[PATH_MAX + 2];
                                snprintf(path, sizeof(path), "G%s\n", pathName);

                                if(write(socketfd, path, strlen(path)) < 0){
                                        fprintf(stderr, "ERROR: Can't send command to server\n");
					close(dataSocket);
                                        continue;
                                }
                                if(readResponse(socketfd, buffer, sizeof(buffer)) != 0){
					close(dataSocket);
					continue;
				}
                                if(debug) printf("DEBUG: Server response: '%s'\n", buffer);

                                if(buffer[0] == 'A'){
					if(debug) printf("DEBUG: Data connection established\n");
                                }
                                else if(buffer[0] == 'E'){
                                	fprintf(stderr, "ERROR: response from server: '%s'\n", (buffer + 1));
					continue;
                                }
				close(dataSocket);
			}
			else{
				fprintf(stderr, "ERROR: proper format is 'get <pathname>'\n");
				continue;
			}
		}
		else if(strcmp(userCommand, "show") == 0){
			if(pathName){
				if(debug) printf("DEBUG: Showing file '%s'\n", pathName);

                                int dataSocket = establishDataConnection(socketfd, debug);
                                if(dataSocket < 0){
                                        fprintf(stderr, "ERROR: Failed to establish data connection\n");
                                        continue;
                                }

                                char path[1024];
                                char buffer[PATH_MAX + 2];
                                snprintf(path, sizeof(path), "G%s\n", pathName);

                                if(write(socketfd, path, strlen(path)) < 0){
                                        fprintf(stderr, "ERROR: Can't send command to server\n");
					close(dataSocket);
                                        continue;
                                }
				if(debug) printf("DEBUG: Waiting for server response\n");

                                if(readResponse(socketfd, buffer, sizeof(buffer)) != 0){
					close(dataSocket);
					continue;
				}
                                if(debug) printf("DEBUG: Server response: '%s'\n", buffer);

                                if(buffer[0] == 'A'){
                                        if(debug) printf("DEBUG: Data connection established\n");
						
					int fd[2];
					pipe(fd);
					pid_t moreFork = fork();
					if(moreFork < 0){
						fprintf(stderr, "ERROR: Fork for 'more' failed\n");
						close(dataSocket);
						continue;
					}
					if(moreFork == 0){
						close(fd[1]);
						dup2(fd[0], 0);
						close(fd[0]);

						execlp("more", "more", "-20", NULL);
						fprintf(stderr, "ERROR: Failed to execute 'more'\n");
						exit(1);
					}
					else{
						close(fd[0]);
						char buffer[4096];
						int numOfBytesRead;

						while((numOfBytesRead = read(dataSocket, buffer, sizeof(buffer))) > 0){
							if(write(fd[1], buffer, numOfBytesRead) < 0){
								fprintf(stderr, "ERROR: Can't write to pipe\n");
								continue;
							}						
						}
						if(numOfBytesRead < 0){
							fprintf(stderr, "ERROR: Can't read data: '%s'\n", strerror(errno));
							continue;
						}
						close(fd[1]);
						close(dataSocket);
						if(debug) printf("DEBUG: waiting for child process to complete execution\n");
						wait(NULL);
						if(debug) printf("DEBUG: Data display and 'more' complete\n");
					}
                                }
                                else if(buffer[0] == 'E'){
                                        fprintf(stderr, "ERROR: response from server: '%s'\n", (buffer + 1));
					close(dataSocket);
					continue;
                                }
                        }
                        else{
                                fprintf(stderr, "ERROR: proper format is 'get <pathname>'\n");
				continue;
                        }
		}
		else if(strcmp(userCommand, "put") == 0){
			if(pathName){
				struct stat area;
				if(stat(pathName, &area) < 0){
					fprintf(stderr, "ERROR: File '%s' is invalid: '%s'\n", pathName, strerror(errno));
					continue;
				}
				if(!S_ISREG(area.st_mode)){
					fprintf(stderr, "ERROR: '%s' is not a regular file\n", pathName);
					continue;
				}

				int fd = open(pathName, O_RDONLY);
				if(fd < 0){
					fprintf(stderr, "ERROR: Can't open file '%s'\n", strerror(errno));
					continue;
				}
				if(debug) printf("DEBUG: Opened local file '%s' for reading\n", pathName);

                                int dataSocket = establishDataConnection(socketfd, debug);
                                if(dataSocket < 0){
                                        fprintf(stderr, "ERROR: Failed to establish data connection\n");
					close(fd);
                                        continue;
                                }

				char baseName[PATH_MAX];
				const char *lastSlash = NULL;
				for(int i = 0; pathName[i] != '\0'; i++){
					if(pathName[i] == '/' && pathName[i + 1] != '\0'){
						lastSlash = &pathName[i];
					}
				}
				if(lastSlash) strncpy(baseName, (lastSlash + 1), (sizeof(baseName) - 1));
				else strncpy(baseName, pathName, (sizeof(baseName) - 1));
				baseName[sizeof(baseName) - 1] = '\0';

                                char path[PATH_MAX + 2];
                                char buffer[PATH_MAX + 2];
                                snprintf(path, sizeof(path), "P%s\n", baseName);

                                if(write(socketfd, path, strlen(path)) < 0){
                                        fprintf(stderr, "ERROR: Can't send command to server\n");
					close(fd);
					close(dataSocket);
                                        continue;
                                }

				if(readResponse(socketfd, buffer, sizeof(buffer)) != 0){
					close(fd);
					close(dataSocket);
					continue;
				}
                                if(debug) printf("DEBUG: Server response: '%s'\n", buffer);

                                if(buffer[0] == 'A'){
                                        if(debug) printf("DEBUG: Data connection established\n");
					char fileBuffer[1024];
					int numOfBytesRead;
					while((numOfBytesRead = read(fd, fileBuffer, sizeof(fileBuffer))) > 0){
						if(write(dataSocket, fileBuffer, numOfBytesRead) < 0){
							fprintf(stderr, "ERROR: Can't write to data connection\n");
							continue;
						}
						if(debug) printf("DEBUG: Writing '%d' bytes to server\n", numOfBytesRead);
					}
					if(numOfBytesRead < 0){
						fprintf(stderr, "ERROR: Can't read data: '%s'\n", strerror(errno));
					}
					printf("'%s' transfered to server, closing local file\n", pathName);
                                }
                                else if(buffer[0] == 'E'){
                                        fprintf(stderr, "ERROR: response from server: '%s'\n", (buffer + 1));
					if(debug) printf("DEBUG: Skipping transmission of file data to server\n");
					continue;
                                }
				close(fd);
                                close(dataSocket);
                        }
                        else{
                                fprintf(stderr, "ERROR: proper format is 'get <pathname>'\n");
				continue;
                        }
		}
		else{
			fprintf(stderr, "ERROR: Command '%s' is invalid\n", userCommand);
			continue;
		}
	}
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
			fprintf(stderr, "ERROR: Server closed connection: '%s'\n", strerror(errno));
			return errno;
		}
		if(buffer[i] == '\n') break;
		i++;
	}
	buffer[i] = '\0';
	return 0;
}

int establishDataConnection(int someSocket, int debug){
	char serverResponse[1024];
	int socketfd;
	struct addrinfo hints, *actualdata;
	char port[16];
	int portNumber;

	if(write(someSocket, "D\n", 2) < 0){
		fprintf(stderr, "ERROR: Can't send 'D' to server: '%s'\n", strerror(errno));
		return 1;
	}

	portNumber = read(someSocket, serverResponse, sizeof(serverResponse) - 1);
	if(portNumber < 0){
		fprintf(stderr, "ERROR: Can't read server response: '%s'\n", strerror(errno));
		return 1;
	}

	serverResponse[strcspn(serverResponse, "\n")] = '\0';
	if(serverResponse[0] != 'A'){
		fprintf(stderr, "ERROR: Server didn't establish data connection\n");
		return 1;
	}

	strncpy(port, (serverResponse + 1), sizeof(port) - 1);
	port[sizeof(port) - 1] = '\0';
	if(debug) printf("DEBUG: Data connection port '%s'\n", port);

	memset(&hints, 0, sizeof(hints));
	int err;

	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_INET;
	err = getaddrinfo(NULL, port, &hints, &actualdata);
	if(err != 0){
		fprintf(stderr, "Error: %s\n", gai_strerror(err));
		return 1;
	}	

	socketfd = socket(actualdata -> ai_family, actualdata -> ai_socktype, 0);
	if(socketfd < 0){
		fprintf(stderr, "ERROR; %s\n", strerror(errno));
		freeaddrinfo(actualdata);
		return 1;
	}
	if(connect(socketfd, actualdata -> ai_addr, actualdata -> ai_addrlen) < 0){
		fprintf(stderr, "Error: %s\n", strerror(errno));
		close(socketfd);
		freeaddrinfo(actualdata);
		return 1;
	}
	freeaddrinfo(actualdata);
	if(debug) printf("DEBUG: Data connection established with socket: '%d'\n", socketfd);
	return socketfd;
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
		fprintf(stderr, "ERROR: %s\n", gai_strerror(err));
		return 1;
	}

	socketfd = socket(actualdata -> ai_family, actualdata -> ai_socktype, 0);
	if(socketfd < 0){
		fprintf(stderr, "ERROR: %s\n", strerror(errno));
		freeaddrinfo(actualdata);
		return errno;
	}
	if(debug) printf("DEBUG: Socket made with descriptor '%d'\n", socketfd);

	if(connect(socketfd, actualdata -> ai_addr, actualdata -> ai_addrlen) < 0){
		fprintf(stderr, "ERROR: Run server first: '%s'\n", strerror(errno));
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
