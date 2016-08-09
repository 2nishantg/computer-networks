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
  int serverFd, clientFd, num;
  socklen_t size;
  struct stat statsBuf;
  char buffer[BUFFER];
  int yes = 1;

  if (argc != 2) { // error if arguments are not correct
    fprintf(stderr, "Usage: %s port port\n", argv[0]);
    exit(1);
  }


  if ((serverFd = socket(AF_INET, SOCK_STREAM, 0)) == -1) { // try to get a socket
    fprintf(stderr, "Socket failure!!\n");
    exit(1);
  }

  if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == 
      -1) { // to enable reuse of port
    perror("setsockopt");
    exit(1);
  }

  memset(&server, 0, sizeof(server)); // Initialize serverInfo struct
  memset(&dest, 0, sizeof(dest));
  server.sin_family = AF_INET;
  server.sin_port = htons(atoi(argv[1]));
  server.sin_addr.s_addr = INADDR_ANY;
  if ((bind(serverFd, (struct sockaddr *)&server, sizeof(struct sockaddr))) ==
      -1) { //try to bind to port
    fprintf(stderr, "Binding Failure\n");
    exit(1);
  }

  if ((listen(serverFd, BACKLOG)) == -1) { // listen for clients
    fprintf(stderr, "Listening Failure\n");
    exit(1);
  }


  while (1) { //outer loop : one iteration per client
    size = sizeof(struct sockaddr_in);
    if ((clientFd = accept(serverFd, (struct sockaddr *)&dest, &size)) ==
        -1) { // accept client
      perror("accept");
      exit(1);
    }
    printf("Server got connection from client %s\n", inet_ntoa(dest.sin_addr));
    FILE *fd = fdopen(clientFd, "r+"); // create stream from socket
    bzero(buffer, BUFFER);
    strcpy(buffer, welcome);
    writeBuffer(buffer, BUFFER, fd);  // welcome message

    while (1) {
      bzero(buffer, BUFFER);
      strcpy(buffer, filePrompt);
      writeBuffer(buffer, BUFFER, fd);  // file prompt
      if ((num = readBuffer(buffer, BUFFER, fd)) == -1) { // read file name
        break;
      } else if (num == -1) {
        printf("Connection closed\n");
        break;
      }
      buffer[num] = '\0';
      printf("Server:Msg Received %s\n", buffer);
      if (access(buffer, R_OK) != -1) { // check if file exists and can be read by process
        stat(buffer, &statsBuf); // find file size
        size = statsBuf.st_size;
        printf("File Size : %d\n", size);
        writeInt(size, fd);  // send file size to client
        writeFile(buffer, size, fd); // send file to client
      } else {
        writeInt(-1, fd); // if file does not exist, send -1 to client
      }
    }
    fclose(fd); // close client stream
  }
  close(serverFd); // close server socket descriptor
  return 0;
}
