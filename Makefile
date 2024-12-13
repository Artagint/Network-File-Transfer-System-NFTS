all: myftp myftpserve

myftp: myftp.o
	gcc -o myftp myftp.o

myftpserve: myftpserve.o myftp.h
	gcc -o myftpserve myftpserve.o

myftp.o: myftp.c myftp.h
	gcc -c myftp.c

myftpserve.o: myftpserve.c myftp.h
	gcc -c myftpserve.c

clean:
	rm -f myftp myftpserve myftp.o myftpserve.o
