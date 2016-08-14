
// AUTHOR: Nishant Gupta


#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define CONNMAX 1000
#define BYTES 1024

char *ROOT = getenv("PWD"), PORT[8];
int listenfd;

enum requestType { GET, POST, HEAD, BAD };
void startServer(char *);
void serveClient(int);
requestType parseHeaders(char *, int &, char *, int &);
int respondGET(char *, int);
int respondPOST(char *, int, int);
int respondHEAD(char *, int);
int sendCommonHeaders(int, int);
int sendNotFound(int, requestType);

int parseArgs(int argc, char *argv[]) {
  char c;
  // Parsing the command line arguments
  while ((c = getopt(argc, argv, "p:r:")) != -1)
    switch (c) {
    case 'r':
      ROOT = (char *)malloc(strlen(optarg));
      strcpy(ROOT, optarg);
      break;
    case 'p':
      strcpy(PORT, optarg);
      break;
    default:
      fprintf(stderr, "%s [-p PORT] [-r ROOT]\n", argv[0]);
      exit(1);
    }
  return 0;
}

int main(int argc, char *argv[]) {
  struct sockaddr_in clientaddr;
  socklen_t addrlen;
  int clientSock;
  strcpy(PORT, "9576");
  parseArgs(argc, argv);

  printf("Server started at port no. %s with root directory as %s\n", PORT,
         ROOT);
  startServer(PORT);

  // ACCEPT connections
  while (1) {
    addrlen = sizeof(clientaddr);
    clientSock = accept(listenfd, (struct sockaddr *)&clientaddr, &addrlen);
    if (clientSock < 0)
      perror("accept() error");
    else {
      if (fork() == 0) {
        serveClient(clientSock);
        exit(0);
      }
    }
  }
  return 0;
}

// start server
void startServer(char *port) {
  struct addrinfo hints, *res, *p;
  int on = 1;
  // taken from `man 3 getaddrinfo`
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // to listen to all interfaces
  if (getaddrinfo(NULL, port, &hints, &res) !=
      0) { // get a list of all interfaces to which socket is bound
    perror("getaddrinfo() error");
    exit(1);
  }
  // try to listen to atleast one
  for (p = res; p != NULL; p = p->ai_next) {
    listenfd = socket(p->ai_family, p->ai_socktype, 0);
    if (listenfd == -1)
      continue;
    if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
      break;
  }
  freeaddrinfo(res);
  // Entire list traversed but could not bind to anyone
  if (p == NULL) {
    perror("socket() or bind()");
    exit(1);
  }

  // free the port as soon as program exits
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  if (listen(listenfd, atoi(port)) != 0) {
    perror("listen() error");
    exit(1);
  }
}

int sendCommonHeaders(int clientSock, int contentLength) {
  char tempBuffer[BYTES], dateBuffer[BYTES];
  send(clientSock, "HTTP/1.1 200 OK\n", 16, 0);
  send(clientSock, "Server: Alchemist\n", 18, 0);
  time_t now = time(0);
  struct tm tm = *gmtime(&now);
  strftime(dateBuffer, sizeof(dateBuffer), "%a, %d %b %Y %H:%M:%S %Z", &tm);
  snprintf(tempBuffer, sizeof(tempBuffer), "Date: %s\n", dateBuffer);
  send(clientSock, tempBuffer, strlen(tempBuffer), 0);
  snprintf(tempBuffer, BYTES, "Content-Length: %d\n", contentLength);
  send(clientSock, tempBuffer, strlen(tempBuffer), 0);
  send(clientSock, "Connection: keep-alive\n\n", 24, 0);
  return 0;
}

int sendNotFound(int clientSock, requestType curRequest) {
  char tempBuffer[1024];
  write(clientSock, "HTTP/1.0 404 Not Found\n", 23); // FILE NOT FOUND
  char *errMesg = (char *)"Not Found\n";
  snprintf(tempBuffer, BYTES, "Content-Length: %d\n", (int)strlen(errMesg));
  send(clientSock, tempBuffer, strlen(tempBuffer), 0);
  send(clientSock, "Connection: keep-alive\n\n", 24, 0);
  if (curRequest != HEAD)
    send(clientSock, errMesg, strlen(errMesg) + 1, 0);
  return 0;
}

int respondGET(char *path, int clientSock) {
  int fd, bytes_read;
  char data_to_send[BYTES];
  struct stat statsBuf;
  printf("file: %s\n", path);
  if ((fd = open(path, O_RDONLY)) != -1) {
    stat(path, &statsBuf); // find file size
    int size = (int)statsBuf.st_size;
    sendCommonHeaders(clientSock, size);
    while ((bytes_read = read(fd, data_to_send, BYTES)) > 0)
      write(clientSock, data_to_send, bytes_read);
  } else {
    sendNotFound(clientSock, GET);
  }
  return 0;
}

int respondHEAD(char *path, int clientSock) {
  int fd;
  struct stat statsBuf;
  printf("\nFile requested : %s\n", path);
  if ((fd = open(path, O_RDONLY)) != -1) {
    stat(path, &statsBuf); // find file size
    int size = (int)statsBuf.st_size;
    sendCommonHeaders(clientSock, size);
  } else {
    sendNotFound(clientSock, HEAD);
  }
  return 0;
}

int respondPOST(char *path, int clientSock, int postPayloadLength) {
  char *postPayload, tempBuffer[BYTES];
  postPayload = strtok(NULL, "");
  postPayload[postPayloadLength] = 0;
  printf("\"%s\" of length %d to be written to %s\n", postPayload,
         (int)strlen(postPayload), path);
  send(clientSock, "HTTP/1.1 200 OK\n", 16, 0);
  snprintf(tempBuffer, BYTES, "Content-Length: %d\n\n", 0);
  send(clientSock, tempBuffer, strlen(tempBuffer) + 1, 0);
  return 1;
}

void serveClient(int clientSock) {
  char mesg[BYTES], path[BYTES];
  int rcvd, keepAliveStatus = 0, payloadLength;
  memset((void *)mesg, 0, BYTES);
  while ((rcvd = recv(clientSock, mesg, 1 << 10, 0)) > 0) {
    if (rcvd < 0) // receive error
      fprintf(stderr, ("recv() error\n"));
    else if (rcvd == 0) // receive socket closed
      fprintf(stderr, "Client disconnected upexpectedly.\n");
    else // message received
    {
      requestType currReq =
          parseHeaders(mesg, keepAliveStatus, path, payloadLength);
      if (currReq == GET)
        respondGET(path, clientSock);
      else if (currReq == HEAD)
        respondHEAD(path, clientSock);
      else if (currReq == POST)
        respondPOST(path, clientSock, payloadLength);
      if (keepAliveStatus)
        break;
    }
  }
  close(clientSock);
}

requestType parseHeaders(char *mesg, int &keepAliveStatus, char *path,
                         int &payloadLength) {
  char *reqline[3], *headerLine;
  requestType curRequest;
  keepAliveStatus = 0;
  reqline[0] = strtok(mesg, " \t");
  if (strncmp(reqline[0], "GET\0", 4) == 0)
    curRequest = GET;
  else if (strncmp(reqline[0], "HEAD\0", 5) == 0)
    curRequest = HEAD;
  else if (strncmp(reqline[0], "POST\0", 5) == 0)
    curRequest = POST;
  reqline[1] = strtok(NULL, " \t");
  reqline[2] = strtok(NULL, "\n");
  if (strncmp(reqline[2], "HTTP/1.1", 8) != 0) {
    return BAD;
  } else {
    if (strncmp(reqline[1], "/\0", 2) == 0) {
      if (curRequest == POST)
        reqline[1] = (char *)"/post_file_test.txt";
      else
        reqline[1] = (char *)"/index.html";
    }
    strcpy(path, ROOT);
    strcpy(&path[strlen(ROOT)], reqline[1]);
  }
  printf("\n");
  headerLine = strtok(NULL, "\n");
  while (strlen(headerLine) > 1) {
    printf("%s\n", headerLine);
    if ((strncmp(headerLine, "Connection: Close", 17) == 0) ||
        (strncmp(headerLine, "Connection: close", 17) == 0))
      keepAliveStatus = 1;
    if (strncmp(headerLine, "Content-Length", 14) == 0) {
      payloadLength = atoi(headerLine + 16);
    }
    headerLine = strtok(NULL, "\n");
  }
  return curRequest;
}
