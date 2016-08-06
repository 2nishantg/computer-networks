
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define PORT 3490
#define MAXSIZE 1024



int sendInt(int num, int fd)
{
  int32_t conv = htonl(num);
  char *data = (char*)&conv;
  int left = sizeof(conv);
  int rc;
  while (left) {
    rc = write(fd, data + sizeof(conv) - left, left);
    if (rc < 0) return -1;
    left -= rc;
  }
  return 0;
}
int receiveInt(int *num, int fd)
{
  int32_t ret, rt;
  char *data = (char*)&ret;
  int left = sizeof(ret);
  while (left) {
    rt = read(fd, data + sizeof(ret) - left, left);
    if (rt < 0) return -1;
    left -= rt;
  }
  *num = ntohl(ret);
  printf("Received %d\n", *num);
  return 0;
}

void prepend(char* s, const char* t)
{
  size_t len = strlen(t);
  size_t i;

  memmove(s + len, s, strlen(s) + 1);

  for (i = 0; i < len; ++i)
    {
      s[i] = t[i];
    }
}

int readFile(char * fileName, int size, int sock) {
  int nread, rtotal = 0;
  FILE* fd;
  char buffer[1024];
  fd = fopen(fileName,"w");
  for(;rtotal < size;) {
    nread = recv(sock, buffer, sizeof(buffer),0);
    rtotal+=nread;
    if (nread != 1024) buffer[nread] = 0;
    //printf("%s", buffer);
    fwrite(buffer, 1, nread, fd);
  }
  fclose(fd);
  return 0;
}

int main(int argc, char *argv[])
{
    struct sockaddr_in server_info;
    struct hostent *he;
    int socket_fd, size;
    char fileName[1024];

    if (argc != 2) {
      fprintf(stderr, "Usage: %s hostname\n", argv[0]);
        exit(1);
    }

    if ((he = gethostbyname(argv[1]))==NULL) {
        fprintf(stderr, "Cannot get host name\n");
        exit(1);
    }

    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0))== -1) {
        fprintf(stderr, "Socket Failure!!\n");
        exit(1);
    }

    memset(&server_info, 0, sizeof(server_info));
    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(PORT);
    server_info.sin_addr = *((struct in_addr *)he->h_addr);

    if (connect(socket_fd, (struct sockaddr *)&server_info, sizeof(struct sockaddr))<0) {
      fprintf(stderr, "Connection Failure\n");
      perror("connect");
      exit(1);
    }


    while(1) {
      printf("Client: Enter File name for Server:");
      fgets(fileName,MAXSIZE-1,stdin);
      fileName[strcspn(fileName, "\n")] = 0;
      if ((send(socket_fd,fileName, strlen(fileName),0))== -1) {
        fprintf(stderr, "Failure Sending Message\n");
        close(socket_fd);
        exit(1);
      }
      else {
        printf("Client:Filename sent: %s\n",fileName);
        receiveInt( &size, socket_fd);
        if(size >= 0)
          {
            printf("Client:File of size %d exists\nClient:Recieving\n", size);
            //            int fd = open("Temp.txt", O_WRONLY);
            prepend(fileName, "ctest/");
            readFile(fileName, size, socket_fd);
          }
        else if (size < 0) {
          printf("Client:File doesn't exist\n");
        }
      }
    }
    close(socket_fd);
    return 0;
}
