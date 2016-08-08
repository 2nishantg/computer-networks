
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

#define PORT 3490
#define MAXSIZE 1024

int main(int argc, char *argv[]) {
  struct sockaddr_in server_info;
  struct hostent *he;
  int socket_fd, size;
  char fileName[1024];

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

  while (1) {
    printf("Client: Enter File name for Server:");
    fgets(fileName, MAXSIZE - 1, stdin);
    fileName[strcspn(fileName, "\n")] = 0;
    if ((send(socket_fd, fileName, strlen(fileName), 0)) == -1) {
      fprintf(stderr, "Failure Sending Message\n");
      close(socket_fd);
      exit(1);
    } else {
      printf("Client:Filename sent: %s\n", fileName);
      receiveInt(&size, socket_fd);
      if (size >= 0) {
        printf("Client:File of size %d exists\nClient:Recieving\n", size);

        struct stat st = {0};
        if (stat("./ctest", &st) == -1) {
          mkdir("/ctest", 0755);
        }
        prepend(fileName, "ctest/");
        readFile(fileName, size, socket_fd);
      } else if (size < 0) {
        printf("Client:File doesn't exist\n");
      }
    }
  }
  close(socket_fd);
  return 0;
}
