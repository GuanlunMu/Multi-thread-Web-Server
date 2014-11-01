/* Client-side use of Berkeley socket calls -- send one message to server
   Requires two command line args:  
     1.  name of host to connect to (use  localhost  to connect to same host)
     2.  port number to use. 
   RAB 3/12 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define MAXBUFF 100

int main(int argc, char **argv) {
  char *prog = argv[0];
  char *host;
  int port;
  int sd;  /* socket descriptor */
  int ret;  /* return value from a call */

  if (argc < 3) {
    printf("Usage:  %s host port\n", prog);
    return 1;
  }
  host = argv[1];
  port = atoi(argv[2]);

  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("%s ", prog);
    perror("socket()");
    return 1;
  }
  struct hostent *hp;
  struct sockaddr_in sa;
  if ((hp = gethostbyname(host)) == NULL) {
    printf("%s ", prog);
    perror("gethostbyname()");
    return 1;
  }
  memset((char *) &sa, '\0', sizeof(sa));
  memcpy((char *) &sa.sin_addr.s_addr, hp->h_addr, hp->h_length);

  /* bzero((char *) &sa, sizeof(sa));*/
  sa.sin_family = AF_INET;
  /* bcopy((char*) sa->h_addr, (char *) &sa.sin_addr.s_addr, hp->h.length */
  sa.sin_port = htons(port);
  
  if ((ret = connect(sd, (struct sockaddr *) &sa, sizeof(sa))) < 0) {
    printf("%s ", prog);
    perror("connect()");
    return 1;
  }
  printf("Connected.\n");  

  char buff[MAXBUFF];  /* message buffer */
  printf("Enter a HTTP request for a file (max %d chars):\n", MAXBUFF-1);
  if (fgets(buff, MAXBUFF, stdin) == NULL) {
    printf("Error or end of input -- aborting\n");
    return 1;
  }
  if ((ret = send(sd, buff, strlen(buff), 0)) < 0) {
    printf("%s ", prog);
    perror("send()");
    return 1;
  }
  char buff2 [MAXBUFF];
  int ret2;
  while ((ret2 = recv(sd, buff2, MAXBUFF-1, 0)) > 0)
    {
      buff2[ret2] = '\0';
      printf("%s", buff2);
    }

  if ((ret = close(sd)) < 0) {
    printf("%s ", prog);
    perror("close()");
    return 1;
  }
  return 0;
}
