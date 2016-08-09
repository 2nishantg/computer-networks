
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
  struct sockaddr_in server_info;
  struct hostent *he;
  int socket_fd, size, num;
  char fileName[BUFFER];

  if (argc != 3) {
    fprintf(stderr, "Usage: %s hostname port\n", argv[0]);
    exit(1);
  }

  if ((he = gethostbyname(argv[1])) == NULL) {
    fprintf(stderr, "Cannot get host name\n");
    exit(1);
  }

  if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    fprintf(stderr, "Socket Failure!!\n");
    exit(1);
  }

  memset(&server_info, 0, sizeof(server_info));
  server_info.sin_family = AF_INET;
  server_info.sin_port = htons(atoi(argv[2]));
  server_info.sin_addr = *((struct in_addr *)he->h_addr);

  if (connect(socket_fd, (struct sockaddr *)&server_info,
              sizeof(struct sockaddr)) < 0) {
    fprintf(stderr, "Connection Failure\n");
    perror("connect");
    exit(1);
  }
  FILE *fd = fdopen(socket_fd,"r+");
  readBuffer(fileName,BUFFER,fd);
  printf("%s",fileName);

  while (1) {
    readBuffer(fileName,BUFFER,fd);
    printf("%s",fileName);
    scanf("%s", fileName);
    if ((writeBuffer(fileName, BUFFER, fd)) == -1) {
      fprintf(stderr, "Failure Sending Message\n");
      close(socket_fd);
      exit(1);
    } else {
      fflush(fd);
      if((num = readInt(&size, fd)) == -1) break;
      if (size > 0) {
        printf("Client:Filename sent: %s\n", fileName);
        printf("Client:File of size %d exists\nClient:Recieving\n", size);
        struct stat st = {0};
        //prepend(fileName, "ctest/");
        readFile(fileName, size, fd);
      } else if (size < 0) {
        printf("Client:Filename sent: %s\n", fileName);
        printf("Client:File doesn't exist\n");
      } else if (size == 0) {
        fclose(fd);
        break;
      }
    }
  }
  close(socket_fd);
  return 0;
}
