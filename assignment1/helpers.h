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

int sendInt(int num, int fd) {
  int32_t conv = htonl(num);
  char *data = (char *)&conv;
  int left = sizeof(conv);
  int rc;
  while (left) {
    rc = write(fd, data + sizeof(conv) - left, left);
    if (rc < 0)
      return -1;
    left -= rc;
  }
  return 0;
}

int receiveInt(int *num, int fd) {
  int32_t ret, rt;
  char *data = (char *)&ret;
  int left = sizeof(ret);
  while (left) {
    rt = read(fd, data + sizeof(ret) - left, left);
    if (rt < 0)
      return -1;
    left -= rt;
  }
  *num = ntohl(ret);
  printf("Received %d\n", *num);
  return 0;
}

void prepend(char *s, const char *t) {
  size_t len = strlen(t);
  memmove(s + len, s, strlen(s) + 1);
  for (size_t i = 0; i < len; ++i)
    s[i] = t[i];
}

int readFile(char *fileName, int size, int sock) {
  int nread, rtotal = 0;
  FILE *fd;
  char buffer[1024];
  fd = fopen(fileName, "w");
  for (; rtotal < size;) {
    nread = recv(sock, buffer, sizeof(buffer), 0);
    rtotal += nread;
    if (nread != 1024)
      buffer[nread] = 0;
    fwrite(buffer, 1, nread, stdout);
    fwrite(buffer, 1, nread, fd);
  }
  fclose(fd);
  return 0;
}

int sendFile(char *fileName, int sock) {
  int nread;
  FILE *fd;
  char buffer[1024];
  fd = fopen(fileName, "r");
  for (nread = fread(buffer, 1, sizeof(buffer), fd); nread > 0;
       nread = fread(buffer, 1, sizeof(buffer), fd)) {
    send(sock, buffer, nread, 0);
  }
  fclose(fd);
  return 0;
}
