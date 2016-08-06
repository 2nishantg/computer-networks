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
#define BACKLOG 10


int sendInt(int num, int fd)
{
  printf("Sending %d\n", num);
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
  int32_t ret;
  char *data = (char*)&ret;
  int left = sizeof(ret);
  int rc;
  while (left) {
    ret = read(fd, data + sizeof(ret) - left, left);
    if (ret < 0) return -1;
    left -= ret;
  }
  *num = ret;
  return 0;
}

int sendFile(char * fileName,  int sock) {
  int nread;
  FILE* fd;
  struct stat stat_buf;      /* argument to fstat */
  char buffer[1024];
  int size;

  fd = fopen(fileName, "r");

  /* get the size of the file to be sent */
  fstat(fd, &stat_buf);
  size = stat_buf.st_size;

  for(nread = fread(buffer, 1, sizeof(buffer), fd); nread > 0; nread = fread(buffer, 1, sizeof(buffer), fd) ) {
    send(sock, buffer, nread, 0);
  }
  //  shutdown(sock, SHUT_WR);
  fclose(fd);
  return 0;
}


int main()
{
    struct sockaddr_in server;
    struct sockaddr_in dest;
    int socket_fd, client_fd,num;
    socklen_t size;
    struct stat stat_buf;      /* argument to fstat */
    char fileName[1024];
    FILE* filed;
//  memset(buffer,0,sizeof(buffer));
    int yes =1;



    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0))== -1) {
        fprintf(stderr, "Socket failure!!\n");
        exit(1);
    }

    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        perror("setsockopt");
        exit(1);
    }
    memset(&server, 0, sizeof(server));
    memset(&dest,0,sizeof(dest));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;
    if ((bind(socket_fd, (struct sockaddr *)&server, sizeof(struct sockaddr )))== -1)    { //sizeof(struct sockaddr)
        fprintf(stderr, "Binding Failure\n");
        exit(1);
    }

    if ((listen(socket_fd, BACKLOG))== -1){
        fprintf(stderr, "Listening Failure\n");
        exit(1);
    }

   while(1) {

        size = sizeof(struct sockaddr_in);

        if ((client_fd = accept(socket_fd, (struct sockaddr *)&dest, &size))==-1 ) {
            perror("accept");
            exit(1);
        }
        printf("Server got connection from client %s\n", inet_ntoa(dest.sin_addr));

        while(1) {
                if ((num = recv(client_fd, fileName, 1024,0))== -1) {
                        perror("recv");
                        exit(1);
                }
                else if (num == 0) {
                        printf("Connection closed\n");
                        //So I can now wait for another client
                        break;
                }
                fileName[num] = '\0';
                printf("Server:Msg Received %s\n", fileName);
                if( access( fileName, R_OK ) != -1 ) {
                  stat(fileName, &stat_buf);
                  size = stat_buf.st_size;
                  printf("File Size : %d\n", size);
                  //                  snprintf ( buffer, 100, "%d", size );
                  // file exists
                  sendInt(size, client_fd);
                  sendFile(fileName, client_fd);
                } else {
                  sendInt(-1, client_fd);
                  // file doesn't exist
                }
                /* if ((send(client_fd,buffer, 1,0))== -1) */
                /* { */
                /*      fprintf(stderr, "Failure Sending Message\n"); */
                /*      close(client_fd); */
                /*      break; */
                /* } /* *\/ */
                /* printf("Server:Msg being sent: %s\nNumber of bytes sent: %d\n",buffer, strlen(buffer)); */
        } //End of Inner While...
        //Close Connection Socket
        close(client_fd);
    } //Outer While
   close(socket_fd);

   return 0;
}
