
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
#include <dirent.h>
#include <assert.h>


#define CONNMAX 1000
#define BYTES 8096
#define min(a, b)  (a < b) ? a : b

char *ROOT = getenv("PWD"), PORT[8];
int listenfd;

enum requestType { GET, POST, HEAD, BAD, UNIMPLEMENTED, ERROR };
void startServer(char *);
void serveClient(int);
requestType parseHeaders(char *, int &, char *, int &);
int respondHG(char *, int, requestType, int);
int respondPOST(char *, int, int, int);
int sendCommonHeaders(int, int, int, char *);
int sendNotFound(int, requestType);
char *generateDirectoryList(char *, int &);

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

int sendCommonHeaders(int clientSock, int contentLength, int keepAliveStatus, char *mimeType) {
  char tempBuffer[BYTES], dateBuffer[BYTES];
  send(clientSock, "HTTP/1.1 200 OK\r\n", 17, 0);
  send(clientSock, "Server: Alchemist\r\n", 19, 0);
  time_t now = time(0);
  struct tm tm = *gmtime(&now);
  strftime(dateBuffer, sizeof(dateBuffer), "%a, %d %b %Y %H:%M:%S %Z", &tm);
  snprintf(tempBuffer, sizeof(tempBuffer), "Date: %s\r\n", dateBuffer);
  send(clientSock, tempBuffer, strlen(tempBuffer), 0);
  snprintf(tempBuffer, BYTES, "Content-Length: %d\r\n", contentLength);
  send(clientSock, tempBuffer, strlen(tempBuffer), 0);
  snprintf(tempBuffer, BYTES, "Content-Type: %s\r\n", mimeType);
  send(clientSock, tempBuffer, strlen(tempBuffer), 0);
  if(keepAliveStatus == 1)   snprintf(tempBuffer, BYTES, "Connection: Close\r\n\r\n");
  else snprintf(tempBuffer, BYTES, "Connection: keep-alive \r\n\r\n");
  send(clientSock, tempBuffer, strlen(tempBuffer), 0);
  return 0;
}

int sendNotFound(int clientSock, requestType curRequest) {
  char tempBuffer[1024];
  write(clientSock, "HTTP/1.0 404 Not Found\n", 23); // FILE NOT FOUND
  char *errMesg = (char *)"<h1>404: Not Found</h1>";
  snprintf(tempBuffer, BYTES, "Content-Length: %d\n", (int)strlen(errMesg));
  send(clientSock, tempBuffer, strlen(tempBuffer), 0);
  snprintf(tempBuffer, BYTES, "Content-Type: text/html\r\n");
  send(clientSock, tempBuffer, strlen(tempBuffer), 0);
  send(clientSock, "Connection: keep-alive\n\n", 24, 0);
  if (curRequest != HEAD)
    send(clientSock, errMesg, strlen(errMesg) + 1, 0);
  return 0;
}

int sendBadRequest(int clientSock, requestType curRequest) {
  char tempBuffer[1024];
  write(clientSock, "HTTP/1.0 400 Bad Request\r\n", 26); // FILE NOT FOUND
  char *errMesg = (char *)"<h1>400: Bad Request</h1>";
  snprintf(tempBuffer, BYTES, "Content-Length: %d\n", (int)strlen(errMesg));
  send(clientSock, tempBuffer, strlen(tempBuffer), 0);
  snprintf(tempBuffer, BYTES, "Content-Type: text/html\r\n");
  send(clientSock, tempBuffer, strlen(tempBuffer), 0);
  send(clientSock, "Connection: keep-alive\r\n\r\n", 26, 0);
  if (curRequest != HEAD)
    send(clientSock, errMesg, strlen(errMesg) + 1, 0);
  return 0;
}

int sendInternalError(int clientSock, requestType curRequest) {
  char tempBuffer[1024];
  write(clientSock, "HTTP/1.0 500 Internal Server Error\r\n", 36); // FILE NOT FOUND
  char *errMesg = (char *)"<h1>500: Internal Server Error</h1>";
  snprintf(tempBuffer, BYTES, "Content-Length: %d\n", (int)strlen(errMesg));
  send(clientSock, tempBuffer, strlen(tempBuffer), 0);
  snprintf(tempBuffer, BYTES, "Content-Type: text/html\r\n");
  send(clientSock, tempBuffer, strlen(tempBuffer), 0);
  send(clientSock, "Connection: close\r\n\r\n", 26, 0);
  if (curRequest != HEAD)
    send(clientSock, errMesg, strlen(errMesg) + 1, 0);
  exit(1);
  return 0;
}

int sendNotImplemented(int clientSock, requestType curRequest) {
  char tempBuffer[1024];
  write(clientSock, "HTTP/1.0 501 Not Implemented\r\n", 30); // FILE NOT FOUND
  char *errMesg = (char *)"<h1>501: Not Implemented</h1>";
  snprintf(tempBuffer, BYTES, "Content-Length: %d\n", (int)strlen(errMesg));
  send(clientSock, tempBuffer, strlen(tempBuffer), 0);
  snprintf(tempBuffer, BYTES, "Content-Type: text/html\r\n");
  send(clientSock, tempBuffer, strlen(tempBuffer), 0);
  send(clientSock, "Connection: keep-alive\r\n\r\n", 26, 0);
  if (curRequest != HEAD)
    send(clientSock, errMesg, strlen(errMesg) + 1, 0);
  return 0;
}

char* generateDirectoryList(char *path, int &size) {
  DIR *dir;
  int idx = 0, written;
  char* retVal = (char *)malloc(99999);
  struct dirent *ent;
  if ((dir = opendir (path)) != NULL) {
    /* print all the files and directories within directory */
    written = sprintf(retVal+idx,"<html><body><ul>\n");
    idx += written;
    while ((ent = readdir (dir)) != NULL) {
      written = sprintf(retVal+idx,"<li><a href=\"%s/%s\">%s</a></li>\n", path + strlen(ROOT),  ent->d_name, ent->d_name);
      idx += written;
    }
    written = sprintf(retVal+idx,"</ul></body></html>\n");
    idx += written;
    size = strlen(retVal);
    closedir(dir);
    return retVal;
  } else {
    /* could not open directory */
    perror ("Could not open directory");
    return NULL;
  }
}

int writeBuffer(int clientSock, char *buffer, int size) {
  int totalWritten = 0, written = 0;
  while(totalWritten < size) {
    written = send(clientSock, buffer + totalWritten, size - totalWritten, 0);
    if(written <= 0 ) return -1;
    totalWritten += written;
  }
  return 0;
}

int respondHG(char *path, int clientSock, requestType curRequest, int keepAliveStatus) {
  int fd, bytes_read, isDirectory, size;
  char data_to_send[BYTES], *directoryList;
  char mimeType[20];
  struct stat statsBuf;
  printf("file: %s\n", path);

  if (stat(path, &statsBuf) == 0) {
    if( statsBuf.st_mode & S_IFDIR ) {
      isDirectory = 1;
      directoryList = generateDirectoryList(path, size);
      strcpy(mimeType, "text/html");
    } else {
      size = (int)statsBuf.st_size;
      char command[100], tempFile[100];
      snprintf(tempFile, 100, "mimeT%d", clientSock);
      snprintf(command, 100,"file --mime-type %s | sed -n 's/.*: \\(.*\\)$/\\1/p' > %s" , path, tempFile);
      system(command);
      FILE * mimeF = fopen(tempFile,"r");
      fscanf(mimeF, "%s", mimeType);
      fclose(mimeF);
      snprintf(command, 100, "rm -f %s", tempFile);
      system(command);
    }
    printf("Mime Type : %s\n", mimeType);
    sendCommonHeaders(clientSock, size, keepAliveStatus, mimeType);
    if(curRequest == GET) {
      if(isDirectory) writeBuffer(clientSock, directoryList, size);
      else {
        fd = open(path, O_RDONLY);
        while ((bytes_read = read(fd, data_to_send, BYTES)) > 0)
          write(clientSock, data_to_send, bytes_read);
      }
    }
  } else {
    sendNotFound(clientSock, curRequest);
  }
  return 0;
}


int respondPOST(char *path, int clientSock, int postPayloadLength, int keepAliveStatus) {
  char postPayload[postPayloadLength], *initialPayload, tempBuffer[BYTES];
  initialPayload = strtok(NULL, "");
  int readTillNow = strlen(initialPayload), recieved;
  FILE *fd = fopen(path, "w");
  strcpy(postPayload, initialPayload);
  while(readTillNow < postPayloadLength) {
    recieved = recv(clientSock, tempBuffer, min(BYTES, postPayloadLength - readTillNow), 0);
    strncpy(postPayload + readTillNow, tempBuffer, min(postPayloadLength - readTillNow, recieved));
    readTillNow += recieved;
  }
  fwrite(postPayload,1,postPayloadLength, fd);
  fflush(fd);
  fclose(fd);
  char *successMesg = (char *)"<h1>Content written Succesfully</h1>";
  send(clientSock, "HTTP/1.1 200 OK\r\n", 17, 0);
  snprintf(tempBuffer, BYTES, "Content-Type: text/html\r\n");
  send(clientSock, tempBuffer, strlen(tempBuffer), 0);
  snprintf(tempBuffer, BYTES, "Content-Length: %d\r\n", (int)strlen(successMesg));
  send(clientSock, tempBuffer, strlen(tempBuffer), 0);
  if(keepAliveStatus == 1)   snprintf(tempBuffer, BYTES, "Connection: Close\r\n\r\n");
  else snprintf(tempBuffer, BYTES, "Connection: keep-alive \r\n\r\n");
  send(clientSock, tempBuffer, strlen(tempBuffer), 0);
  send(clientSock, successMesg, strlen(successMesg), 0);
  return 1;
}

void serveClient(int clientSock) {
  char mesg[BYTES], path[BYTES];
  int rcvd, keepAliveStatus = 0, payloadLength, idx = 1;
  memset((void *)mesg, 0, BYTES);
  fprintf(stderr, "Connected to client %d\n", clientSock);
  while ((rcvd = recv(clientSock, mesg, BYTES - 1, 0)) > 0) {
    fprintf(stderr, "\nRequest %d from Client %d\n", idx++, clientSock);
      requestType currReq =
          parseHeaders(mesg, keepAliveStatus, path, payloadLength);
      if (currReq == GET || currReq == HEAD)
        respondHG(path, clientSock, currReq, keepAliveStatus);
      else if (currReq == POST)
        respondPOST(path, clientSock, payloadLength, keepAliveStatus);
      else if (currReq == UNIMPLEMENTED) sendNotImplemented(clientSock, currReq);
      else if (currReq == BAD) sendBadRequest(clientSock, currReq);
      else if (currReq == ERROR) sendInternalError(clientSock, currReq);
      if (keepAliveStatus)
        break;
  }
  fprintf(stderr, "Closed Client\n");
  close(clientSock);
}

requestType parseHeaders(char *mesg, int &keepAliveStatus, char *path,
                         int &payloadLength) {
  char *reqline[3], *headerLine;
  try {
    requestType curRequest;
    keepAliveStatus = 0;
    reqline[0] = strtok(mesg, " \t");
    if (strncmp(reqline[0], "GET\0", 4) == 0)
      curRequest = GET;
    else if (strncmp(reqline[0], "HEAD\0", 5) == 0)
      curRequest = HEAD;
    else if (strncmp(reqline[0], "POST\0", 5) == 0)
      curRequest = POST;
    else if (strncmp(reqline[0], "PUT\0", 4) == 0 ||
             strncmp(reqline[0], "DELETE\0", 7) == 0 ||
             strncmp(reqline[0], "CONNECT\0", 8) == 0 ||
             strncmp(reqline[0], "OPTIONS\0", 8) == 0)
      return UNIMPLEMENTED;
    else return BAD;
    reqline[1] = strtok(NULL, " \t");
    reqline[2] = strtok(NULL, "\n");
    printf("%s %s %s", reqline[0],reqline[1], reqline[2]);
    if ((strncmp(reqline[2], "HTTP/1.1", 8) != 0) &&
        (strncmp(reqline[2], "HTTP/1.0", 8) != 0)) {
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
  } catch(...) {return ERROR;}
}
