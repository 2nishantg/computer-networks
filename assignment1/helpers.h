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


#define BACKLOG 100 // no of connections by server
#define BUFFER 1024 // buffer size used for transmitting data
#define min(a, b) (a < b) ? a : b
#define miax(a, b) (a > b) ? a : b


// writes size bytes starting from buffer to fd
int writeBuffer(char *buffer, size_t size, FILE *fd) {
  int sent = 0, totalSent = 0;
  while (totalSent < size) { // keep resending till data is not sent
    if ((sent = fwrite(buffer + totalSent, 1, size - totalSent, fd)) == 0) { // if it is 0, then error has occured
      printf("Remote End closed\n");
      return -1;
    }
    totalSent += sent;
  }
  fflush(fd); // needed if buffer size if large
  return totalSent;
}

// reads size bytes into buffer from fd
int readBuffer(char *buffer, size_t size, FILE *fd) {
  int received = 0, totalReceived = 0;
  while (totalReceived < size) {
    if ((received = fread(buffer + totalReceived, 1, size - totalReceived,
                          fd)) == 0) { // if it is 0, then error has occured
      printf("Remote End closed\n");
      return -1;
    }
    totalReceived += received;
  }
  fflush(fd); // needed if buffer size is large
  return totalReceived;
}

//sends and integer that can be read by below function
int writeInt(int num, FILE *fd) {
  char buffer[BUFFER];
  sprintf(buffer,"%d",num);
  writeBuffer(buffer,BUFFER, fd);
  return 0;
}

//complementry to above function
int readInt(int *num, FILE *fd) {
  char buffer[BUFFER];
  bzero(buffer,BUFFER);
  readBuffer(buffer, BUFFER, fd);
  *num = atoi(buffer);
  return 0;
}

// readFile and writeFile always send BUFFER bytes, but in the last packet
// only (size - received_till_now) are displayed

//readFile sent using writeFile, and print it to stdout.
int readFile(int size, FILE *fd) {
  int nread, rtotal = 0;
  char buffer[BUFFER];
  while (rtotal < size) {
    //bzero(buffer, sizeof(buffer));
    nread = readBuffer(buffer, sizeof(buffer), fd);
    fwrite(buffer, 1, min(size - rtotal, sizeof(buffer)), stdout);
    rtotal += nread;
  }
  return 0;
}


//writes File to a socket that can be read by readfile above
int writeFile(char *fileName, size_t size, FILE *fd) {
  int nread, rtotal = 0;
  FILE *filed;
  char buffer[BUFFER];
  filed = fopen(fileName, "r");
  while (rtotal < size) {
    //bzero(buffer, sizeof(buffer));
    nread = fread(buffer, 1, sizeof(buffer), filed);
    rtotal += nread;
    writeBuffer(buffer, sizeof(buffer), fd);
  }
  fclose(filed);
  return 0;
}
