
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "helpers.h"

int main(int argc, char *argv[]) {
  struct sockaddr_in serverInfo;
  struct hostent *he;
  int socketFd, size, num;
  char buffer[BUFFER];

  if (argc != 3) { // error if arguments are not correct
    fprintf(stderr, "Usage: %s hostname port\n", argv[0]);
    exit(1);
  }

  if ((he = gethostbyname(argv[1])) == NULL) { // get host from name
    fprintf(stderr, "Cannot get host name\n");
    exit(1);
  }

  if ((socketFd = socket(AF_INET, SOCK_STREAM, 0)) ==
      -1) { // try to get a socket
    fprintf(stderr, "Socket Failure!!\n");
    exit(1);
  }

  memset(&serverInfo, 0, sizeof(serverInfo)); // Initialize serverInfo struct
  serverInfo.sin_family = AF_INET;
  serverInfo.sin_port = htons(atoi(argv[2]));
  serverInfo.sin_addr = *((struct in_addr *)he->h_addr);

  if (connect(socketFd, (struct sockaddr *)&serverInfo,
              sizeof(struct sockaddr)) < 0) { // try to connect socket to server
    fprintf(stderr, "Connection Failure\n");
    perror("connect");
    exit(1);
  }
  FILE *fd =
      fdopen(socketFd, "r+"); // convert socket to file stream for fread/fwrite
  readBuffer(buffer, BUFFER, fd); // read Welcome message
  printf("%s", buffer);           // print Welcome message

  while (1) {
    readBuffer(buffer, BUFFER, fd); // read file Prompt
    printf("%s", buffer);           // print file Prompt
    scanf("%[^\n]%*c",
          buffer); // scan file name, this pattern is to support spaces
    if ((writeBuffer(buffer, BUFFER, fd)) == -1) { // send file name
      fprintf(stderr, "Failure Sending Message\n");
      close(socketFd); // close if error and exit
      exit(1);
    } else {
      fflush(fd); // flush the stream
      if ((num = readInt(&size, fd)) == -1)
        break;        // if could not read an integer, then stream is closed
      if (size > 0) { // if size > 0, then file found
        printf("Client:Filename sent: %s\n", buffer);
        printf("Client:File of size %d exists\nClient:Recieving\n", size);
        readFile(size, fd);
      } else if (size < 0) {
        printf("Client:Filename sent: %s\n", buffer);
        printf("Client:File doesn't exist\n");
      } else if (size == 0) { // size is 0 only if could not read
        fclose(fd);           // see readInt() in helpers.h
        break;
      }
    }
  }
  close(socketFd); // close socket
  return 0;
}
