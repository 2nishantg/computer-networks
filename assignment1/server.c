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


char *welcome = "Welcome to Alchemist!\n";
char *filePrompt = "Enter the filename of file you want to download: \0";

int main(int argc, char *argv[]) {
  struct sockaddr_in server;
  struct sockaddr_in dest;
  int socket_fd, client_fd, num;
  socklen_t size;
  struct stat stat_buf;
  char fileName[BUFFER];
  int yes = 1;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s port port\n", argv[0]);
    exit(1);
  }


  if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    fprintf(stderr, "Socket failure!!\n");
    exit(1);
  }

  if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) ==
      -1) {
    perror("setsockopt");
    exit(1);
  }
  memset(&server, 0, sizeof(server));
  memset(&dest, 0, sizeof(dest));
  server.sin_family = AF_INET;
  server.sin_port = htons(atoi(argv[1]));
  server.sin_addr.s_addr = INADDR_ANY;
  if ((bind(socket_fd, (struct sockaddr *)&server, sizeof(struct sockaddr))) ==
      -1) {
    fprintf(stderr, "Binding Failure\n");
    exit(1);
  }

  if ((listen(socket_fd, BACKLOG)) == -1) {
    fprintf(stderr, "Listening Failure\n");
    exit(1);
  }


  while (1) {
    size = sizeof(struct sockaddr_in);
    if ((client_fd = accept(socket_fd, (struct sockaddr *)&dest, &size)) ==
        -1) {
      perror("accept");
      exit(1);
    }
    printf("Server got connection from client %s\n", inet_ntoa(dest.sin_addr));
    FILE *fd = fdopen(client_fd, "r+");
    bzero(fileName, BUFFER);
    strcpy(fileName, welcome);
    writeBuffer(fileName, BUFFER, fd);

    while (1) {
      bzero(fileName, BUFFER);
      strcpy(fileName, filePrompt);
      writeBuffer(fileName, BUFFER, fd);
      if ((num = readBuffer(fileName, BUFFER, fd)) == -1) {
        break;
      } else if (num == -1) {
        printf("Connection closed\n");
        break;
      }
      fileName[num] = '\0';
      printf("Server:Msg Received %s\n", fileName);
      if (access(fileName, R_OK) != -1) {
        stat(fileName, &stat_buf);
        size = stat_buf.st_size;
        printf("File Size : %d\n", size);
        writeInt(size, fd);
        writeFile(fileName, size, fd);
      } else {
        writeInt(-1, fd);
      }
    }
    close(client_fd);
  }
  close(socket_fd);
  return 0;
}
