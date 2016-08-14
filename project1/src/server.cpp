/*
AUTHOR: Nishant Gupta
*/

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

char *ROOT;
int listenfd, clients[CONNMAX];

enum requestType { GET, POST, HEAD, BAD };
void startServer(char *);
void serveClient(int);
// (request, keepAliveStatus, fileName to serve)
requestType parseHeaders(char *, int &, char *, int &);
int respondGET(char *, int);
int respondPOST(char *, int, int);
int respondHEAD(char *, int);


int main(int argc, char *argv[]) {
  struct sockaddr_in clientaddr;
  socklen_t addrlen;
  char c;

  // Default Values PATH = PWD and PORT=10000
  char PORT[6];
  ROOT = getenv("PWD");
  strcpy(PORT, "9576");

  int slot = 0;

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
    case '?':
      fprintf(stderr, "Wrong arguments given!!!\n");
      exit(1);
    default:
      exit(1);
    }

  printf("Server started at port no. %s%s%s with root directory as %s%s%s\n",
         "\033[92m", PORT, "\033[0m", "\033[92m", ROOT, "\033[0m");
  // Setting all elements to -1: signifies there is no client connected
  int i;
  for (i = 0; i < CONNMAX; i++)
    clients[i] = -1;
  startServer(PORT);

  // ACCEPT connections
  while (1) {
    while (clients[slot] != -1)
      slot = (slot + 1) % CONNMAX;
    addrlen = sizeof(clientaddr);
    clients[slot] = accept(listenfd, (struct sockaddr *)&clientaddr, &addrlen);

    if (clients[slot] < 0)
      perror("accept() error");
    else {
      if (fork() == 0) {
        serveClient(slot);
        exit(0);
      }
    }
  }

  return 0;
}

// start server
void startServer(char *port) {
  struct addrinfo hints, *res, *p;

  // getaddrinfo for host
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  if (getaddrinfo(NULL, port, &hints, &res) != 0) {
    perror("getaddrinfo() error");
    exit(1);
  }
  // socket and bind
  for (p = res; p != NULL; p = p->ai_next) {
    listenfd = socket(p->ai_family, p->ai_socktype, 0);
    if (listenfd == -1)
      continue;
    if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
      break;
  }
  if (p == NULL) {
    perror("socket() or bind()");
    exit(1);
  }
  int on = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  freeaddrinfo(res);

  if (listen(listenfd, atoi(port)) != 0) {
    perror("listen() error");
    exit(1);
  }
}

int respondGET(char *path, int n) {
  int fd, bytes_read;
  char data_to_send[BYTES], tempBuffer[BYTES], dateBuffer[BYTES];
  struct stat statsBuf;
  printf("file: %s\n", path);
  if ((fd = open(path, O_RDONLY)) != -1) {
    stat(path, &statsBuf); // find file size
    int size = (int)statsBuf.st_size;
    send(clients[n], "HTTP/1.1 200 OK\n", 16, 0);
    send(clients[n], "Server: Alchemist\n", 18, 0);
    time_t now = time(0);
    struct tm tm = *gmtime(&now);
    strftime(dateBuffer, sizeof(dateBuffer), "%a, %d %b %Y %H:%M:%S %Z", &tm);
    snprintf(tempBuffer, sizeof(tempBuffer), "Date: %s\n", dateBuffer);
    send(clients[n], tempBuffer, strlen(tempBuffer), 0);
    snprintf(tempBuffer, BYTES, "Content-Length: %d\n", size);
    send(clients[n], tempBuffer, strlen(tempBuffer), 0);
    send(clients[n], "Connection: keep-alive\n\n", 24, 0);
    while ((bytes_read = read(fd, data_to_send, BYTES)) > 0)
      write(clients[n], data_to_send, bytes_read);
  } else {
    write(clients[n], "HTTP/1.0 404 Not Found\n", 23); // FILE NOT FOUND
    char *errMesg = (char *)"Not Found\n";
    snprintf(tempBuffer, BYTES, "Content-Length: %d\n", (int)strlen(errMesg));
    send(clients[n], tempBuffer, strlen(tempBuffer), 0);
    send(clients[n], "Connection: keep-alive\n\n", 24, 0);
    send(clients[n], errMesg, strlen(errMesg), 0);
  }
  return 0;
}

int respondHEAD(char *path, int n) {
  int fd;
  char tempBuffer[BYTES], dateBuffer[BYTES];
  struct stat statsBuf;
  printf("file: %s\n", path);
  if ((fd = open(path, O_RDONLY)) != -1) {
    stat(path, &statsBuf); // find file size
    int size = (int)statsBuf.st_size;
    send(clients[n], "HTTP/1.1 200 OK\n", 16, 0);
    send(clients[n], "Server: Alchemist\n", 18, 0);
    time_t now = time(0);
    struct tm tm = *gmtime(&now);
    strftime(dateBuffer, sizeof(dateBuffer), "%a, %d %b %Y %H:%M:%S %Z", &tm);
    snprintf(tempBuffer, sizeof(tempBuffer), "Date: %s\n", dateBuffer);
    send(clients[n], tempBuffer, strlen(tempBuffer), 0);
    snprintf(tempBuffer, BYTES, "Content-Length: %d\n", size);
    send(clients[n], tempBuffer, strlen(tempBuffer), 0);
    send(clients[n], "Connection: keep-alive\n\n", 24, 0);
  } else {
    send(clients[n], "HTTP/1.0 404 Not Found\n", 23, 0); // FILE NOT FOUND
    snprintf(tempBuffer, BYTES, "Content-Length: %d\n", 0);
    send(clients[n], tempBuffer, strlen(tempBuffer), 0);
    send(clients[n], "Connection: keep-alive\n\n", 24, 0);
  }
  return 0;
}


int respondPOST(char *path, int n, int postPayloadLength) {
  char *postPayload, tempBuffer[BYTES];
  postPayload = strtok(NULL,"");
  postPayload[postPayloadLength] = 0;
  printf("\"%s\" of length %d to be written to %s\n",postPayload, (int)strlen(postPayload), path);
  send(clients[n], "HTTP/1.1 200 OK\n", 16, 0);
  snprintf(tempBuffer, BYTES, "Content-Length: %d\n\n", 9);
  send(clients[n], tempBuffer, strlen(tempBuffer)+1, 0);
  snprintf(tempBuffer, BYTES, "Not Found");
  send(clients[n], tempBuffer, strlen(tempBuffer), 0);
  return 1;
}

void serveClient(int n) {
  char mesg[1 << 10], path[1 << 10];
  int rcvd, idx = 0, keepAliveStatus = 0, payloadLength;

  memset((void *)mesg, 0, 1 << 10);

  while ((rcvd = recv(clients[n], mesg, 1 << 10, 0)) > 0) {
    printf("%d Client: %d\n", n, idx++);
    if (rcvd < 0) // receive error
      fprintf(stderr, ("recv() error\n"));
    else if (rcvd == 0) // receive socket closed
      fprintf(stderr, "Client disconnected upexpectedly.\n");
    else // message received
    {
      requestType currReq = parseHeaders(mesg, keepAliveStatus, path, payloadLength);
      if (currReq == GET)
        respondGET(path, n);
      else if (currReq == HEAD)
        respondHEAD(path, n);
      else if (currReq == POST)
        respondPOST(path, n, payloadLength);
      if(keepAliveStatus) break;
    }
  }
  close(clients[n]);
  clients[n] = -1;
}

requestType parseHeaders(char *mesg, int &keepAliveStatus, char *path, int &payloadLength) {
  char *reqline[3], *headerLine;
  requestType curRequest;
  keepAliveStatus = 0;
  reqline[0] = strtok(mesg, " \t");
  if (strncmp(reqline[0], "GET\0", 4) == 0) curRequest = GET;
  else if (strncmp(reqline[0], "HEAD\0", 5) == 0) curRequest = HEAD;
  else if (strncmp(reqline[0], "POST\0", 5) == 0) curRequest = POST;
  reqline[1] = strtok(NULL, " \t");
  reqline[2] = strtok(NULL, "\n");
  if (strncmp(reqline[2], "HTTP/1.1", 8) != 0) {
    return BAD;
  } else {
    if (strncmp(reqline[1], "/\0", 2) == 0)
      reqline[1] = (char *)"/index.html";
    strcpy(path, ROOT);
    strcpy(&path[strlen(ROOT)], reqline[1]);
  }
  headerLine = strtok(NULL, "\n");
  while (strlen(headerLine) > 1) {
    printf("%s\n", headerLine);
    if ((strncmp(headerLine, "Connection: Close", 17) == 0) ||
        (strncmp(headerLine, "Connection: close", 17) == 0))
      keepAliveStatus = 1;
    if (strncmp(headerLine, "Content-Length", 14) == 0) {
        payloadLength = atoi(headerLine+16);
      }
    headerLine = strtok(NULL, "\n");
  }
  return curRequest;
}
