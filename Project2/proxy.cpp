#include "proxy_parse.h"
#include <arpa/inet.h>
#include <assert.h>
#include <dirent.h>
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
#define BYTES 8096
#define min(a, b) (a < b) ? a : b



void startServer(char *);
void serveClient(int);

char *ROOT = getenv("PWD"), PORT[8];
int listenfd;

int main(int argc, char * argv[]) {
  struct sockaddr_in clientaddr;
  socklen_t addrlen;
  int clientSock;
  strcpy(PORT, argv[1]);
  startServer(PORT);
  //  printf("Proxy server started at port no. %s\nProxy running", PORT);
  setbuf(stdout, NULL);
  while (1) {
    addrlen = sizeof(clientaddr);
    clientSock = accept(listenfd, (struct sockaddr *)&clientaddr, &addrlen);
    if (clientSock < 0)
      perror("accept() error");
    else {
      if (fork() == 0) {
        serveClient(clientSock);
        exit(0);
      } else close(clientSock);
    }
  }

  return 0;
}


// starts the server, One time call
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

// Write size bytes form buffer to clientSock
int writeBuffer(int clientSock, char *buffer, int size) {
  int totalWritten = 0, written = 0;
  while (totalWritten < size) {
    written = send(clientSock, buffer + totalWritten, size - totalWritten, 0);
    if (written <= 0)
      return -1;
    totalWritten += written;
  }
  return 0;
}

int response(int clientSock, int socketFd) {
  char mesg[BYTES];
  int rcvd,totalSent, sent;
  while ((rcvd = recv(socketFd, mesg, BYTES - 1, 0)) > 0) {
    totalSent = 0;
    while(totalSent < rcvd) {
      sent = send(clientSock, mesg+totalSent, rcvd - totalSent, 0);
      if(sent <= 0) return -1;
      totalSent+=sent;
    }
  }
  close(clientSock);
  return 1;
}

int memsearch(const char *hay, int haysize, const char *needle, int needlesize) {
  int haypos, needlepos;
  haysize -= needlesize;
  for (haypos = 0; haypos <= haysize; haypos++) {
    for (needlepos = 0; needlepos < needlesize; needlepos++) {
      if (hay[haypos + needlepos] != needle[needlepos]) {
        // Next character in haystack.
        break;
      }
    }
    if (needlepos == needlesize) {
      return haypos;
    }
  }
  return -1;
}


// Main function that is called after a fork().
void serveClient(int clientSock) {
  char mesg[BYTES];
  int rcvd, offset=0, currentBlank = 0;
  memset((void *)mesg, 0, BYTES);
  while ((rcvd = recv(clientSock, mesg+offset, BYTES - 1 - offset, 0)) > 0) {
    //    printf("%.*s", rcvd, mesg + offset);
    if( rcvd == 2 && strncmp(mesg+offset, "\r\n", 2) == 0){
      if(offset != 0) currentBlank++;
      else offset -= rcvd; // remove stray blank lines
    } else currentBlank = 0;
    offset += rcvd;
    if(currentBlank == 2 || (offset >= 4 && strncmp(mesg+offset-4,"\r\n\r\n", 4) == 0)) {
      mesg[offset] = 0;
      //      fprintf(stdout, "%d \n%s", offset, mesg);
      //      fflush(stdout);
      ParsedRequest *req = ParsedRequest_create();
      struct hostent *he;
      struct sockaddr_in serverInfo;
      int socketFd, headerEnd;
      if (ParsedRequest_parse(req, mesg, offset, &headerEnd) < 0) {
        printf("parse failed\n");
      } else {
        if ((he = gethostbyname(req->host)) == NULL) { // get host from name
          fprintf(stderr, "Cannot get host name\n");
        } else {
          if(req->port == NULL) req->port = (char *)"80";
          memset(&serverInfo, 0, sizeof(serverInfo)); // Initialize serverInfo struct
          serverInfo.sin_family = AF_INET;
          serverInfo.sin_port = htons(atoi(req->port));
          serverInfo.sin_addr = *((struct in_addr *)he->h_addr);
          if ((socketFd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            fprintf(stderr, "Socket Failure!!\n");
          } else if (connect(socketFd, (struct sockaddr *)&serverInfo,
                      sizeof(struct sockaddr)) < 0) {
            fprintf(stderr, "Connection Failure\n");
            perror("connect");
          } else {
            if (ParsedHeader_set(req, "Connection", "Close") < 0){
              printf("Could not set connection to close\n");
            }
            int rlen = ParsedRequest_totalLen(req);
            char *buf = (char *)malloc(rlen+1);
            if (ParsedRequest_unparse(req, buf, rlen) < 0) {
              printf("unparse failed\n");
            } else {
              writeBuffer(socketFd, buf, rlen);
              // buf[rlen] ='\0';
              // printf( "%s\n", buf);
              response(clientSock, socketFd);
            }
            close(socketFd);
          }
        }
        ParsedRequest_destroy(req);
      }
      offset = 0;
      currentBlank = 0;
    }
  }
  //  fprintf(stderr, "Closed Client\n");
  close(clientSock);
  return;
}
