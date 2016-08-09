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
#include <stdlib.h>


#define BACKLOG 10
#define BUFFER 1024
#define min(a, b) (a < b) ? a : b
#define miax(a, b) (a > b) ? a : b



int writeBuffer(char *buffer, size_t size, FILE *fd) {
  int sent = 0, total_sent = 0;
  while (total_sent < size) {
    if ((sent = fwrite(buffer + total_sent, 1, size - total_sent, fd)) == 0) {
      return -1;
    }
    total_sent += sent;
  }
  fflush(fd);
  return total_sent;
}

int readBuffer(char *buffer, size_t size, FILE *fd) {
  int received = 0, total_received = 0;
  while (total_received < size) {
    if ((received = fread(buffer + total_received, 1, size - total_received,
                          fd)) == 0) {
      printf("Remote End closed\n");
      return -1;
    }
    total_received += received;
  }
  fflush(fd);
  return total_received;
}


int writeInt(int num, FILE *fd) {
  char buffer[BUFFER];
  sprintf(buffer,"%d",num);
  writeBuffer(buffer,BUFFER, fd);
  return 0;
}

int readInt(int *num, FILE *fd) {
  char buffer[BUFFER];
  bzero(buffer,BUFFER);
  readBuffer(buffer, BUFFER, fd);
  *num = atoi(buffer);
  return 0;
}

void prepend(char *s, const char *t) {
  size_t len = strlen(t);
  memmove(s + len, s, strlen(s) + 1);
  for (size_t i = 0; i < len; ++i)
    s[i] = t[i];
}

int readFile(char *fileName, int size, FILE *fd) {
  int nread, rtotal = 0;
  FILE *filed = fopen(fileName, "w");
  char buffer[BUFFER];
  while (rtotal < size) {
    bzero(buffer, sizeof(buffer));
    nread = readBuffer(buffer, sizeof(buffer), fd);
    fwrite(buffer, 1, min(size - rtotal, sizeof(buffer)), stdout);
    rtotal += nread;
  }
  fclose(filed);
  return 0;
}

int writeFile(char *fileName, size_t size, FILE *fd) {
  int nread, rtotal = 0;
  FILE *filed;
  char buffer[BUFFER];
  filed = fopen(fileName, "r");
  while (rtotal < size) {
    bzero(buffer, sizeof(buffer));
    nread = fread(buffer, 1, sizeof(buffer), filed);
    rtotal += nread;
    writeBuffer(buffer, sizeof(buffer), fd);
  }
  fclose(filed);
  return 0;
}
