/* Server-side use of Berkeley socket calls -- receive one message and print 
   Requires one command line arg:  
     1.  port number to use (on this machine). 
   RAB 3/12 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <string.h>


#define MAXBUFF 100
#define THREAD_NO 20
#define MAXTOKS 100
#define BUFFSIZE 500
#define MAXTOKSIZE 100

int next_request_id;
pthread_mutex_t mutexLock;

struct tdata
{
  int ssockd;
  char * prog;
  int id;
  int status;
  int count;
  int * reqid; // id pointer for next request
  time_t * time;
  pthread_mutex_t * gLock;
} tdarray[THREAD_NO];


struct name
{
  char** tok;
  int count;
  int status;
};


enum status_value { NORMAL, EOF_FOUND, INPUT_OVERFLOW ,OVERSIZE_TOKEN, BAD_REQUEST, NOT_FOUND, NOT_IMPLEMENTED};


int read_name(struct name *token)
{
  char buffer[BUFFSIZE];
  char * result;
  // printf("Guanlun's Shell: ");
  result = fgets(buffer,BUFFSIZE, stdin);
  token -> tok = (char**) malloc(MAXTOKS);
  int token_no=0;  //the nth token in loading and it implies the number of token in struct token
  
  int n;            //it's the nth element in token_noth of token->tok
  int buff_no = 0;  //it keeps track of the nth element in buffer[]
  
  while (buffer[buff_no]!= 0) 
    {
      token -> tok[token_no] = (char*) malloc(MAXTOKSIZE+1);
      n = 0;
      while(!isspace(buffer[buff_no]))
	{
	  token -> tok[token_no][n] = buffer[buff_no];
	  n++;
	  buff_no++;
	}
      token -> tok[token_no][n]= 0; //add a null byte at the end of the tok
    
      //what about if there is too much tokens?
      if( token_no >= MAXTOKS)
	{
	  token -> status = INPUT_OVERFLOW;
	  break;
	}

      //what about if there is too much char in a sigle token?
      if(n > MAXTOKSIZE)
	{
	  token -> status = OVERSIZE_TOKEN;
	  break;
	}
      else if (result != NULL)
	  token -> status = NORMAL ;
      else 
	  token -> status = EOF_FOUND;
      
      token_no++;
      
      // an inner small loop that skip all white space:
      int i;
      while(isspace(buffer[buff_no]))
	{
	  i++;
	  buff_no++;
	}
    }
  token -> count = token_no;
  /* if (token -> status == NORMAL)
    return 1;
  else 
  return 0;*/
}

void Header(int status, char * date, int count)
{
  FILE * f;
  f = fopen("header.txt", "w");
  if (status == 404)
    {
      fputs("\nHTTP/1.1 404 Not Found\r\n", f);
      fprintf(f, "%s", date);
      fputs("\r\nConection: close\r\nContent-Type: text/heml; charset = utf-8\r\ncontent-Length: 0", f);
      fputs("\r\n\r\n", f);
    }
  else if (status == 501)
    {
      fputs("\nHTTP/1.1 501 Not Implemented\r\n", f);    
      fputs("\r\nConection: close\r\nContent-Type: text/heml; charset = utf-8\r\ncontent-Length: 0", f);
      fputs("\r\n\r\n", f);
    }
  else if (status == 200)
    {
     fputs("\nHTTP/1.1 200 OK\nDate: ", f);
     fprintf(f, "%s", date);
     fputs("\r\nConection: close\r\nContent-Type: text/heml; charset = utf-8\r\ncontent-Length: 0", f);
     fprintf(f, "%d", count);
     fputs("\r\n\r\n", f);
    }
  fclose (f);
}

void * process_requests(void * tdatarg)
{
  struct tdata td = *((struct tdata *) tdatarg);
  int clientd;
  struct sockaddr_in ca;
  int size  = sizeof (struct sockaddr);
 
  //open socket for communication
  printf("Waiting for incoming client...\n");
  if ((clientd = accept(td.ssockd, (struct sockaddr*) & ca, &size)) <0)
    {
      printf("%s ",td.prog);
      td.status = 400;
      exit(1);
    }

  //recieve message from client
  char * buff = malloc(MAXBUFF);
  int ret; 
  if ((ret = recv(clientd, buff, MAXBUFF-1, 0)) < 0)
    {
      printf("%s ", td.prog);
      td.status = 400;
      exit(1);
    }

  //parsing the request
  struct name nm;
  int stat;
  stat = read_name(&nm);

  buff[ret] = '\0';

  printf("Received message (%d chars): \n%s", ret, buff);
  //tracking the time
  time_t now;
  char timestamp[30];
  now = time(NULL);
  strftime(timestamp, 30, "a%, %d %b %Y %T %Z", gmtime(&now));

  //get the reauest!
  if (!strcmp(nm.tok[0], "GET"))
    {
      FILE * pFile;
      char buffer[100];
      char buffer2[100];
      pFile = fopen(&(nm.tok[1][1]), "r");
      if (pFile == NULL)
	{
	  td.status = 404;
	  Header(td.status, timestamp, td.count);
	  FILE * head;
	  head = fopen("header.txt", "r");

	  while (!feof(head))
	    {
	      if (fgets(buffer, 100, head) != NULL)
		{
		  if ((ret = send(clientd, buffer, strlen(buffer), 0)) < 0)
		    {
		      printf("%s ", td.prog);
		      td.status = 400;
		      exit(1);
		    }
		}
	    }
	  fclose(head);
	}
      else  // file exist
	{
	  td.status = 200;
	  int st;
	  struct stat fs; 
	  st = fstat(fileno(pFile), &fs);
	  Header(td.status, timestamp, td.count);
	  FILE * head;
	  head = fopen("header.txt", "r");
	  while(!feof(head))
	    {
	      if(fgets(buffer, 100, head) != NULL)
		{
		  if ((ret = send(clientd, buffer, strlen(buffer), 0))< 0)
		    {
		      printf("%s ", td.prog);
		      td.status = 400;
		      exit(1);
		    }
		}
	    }
	  while(!feof(head))
	    {
	      if(fgets(buffer2, 100, pFile) != NULL)
		{
		  if ((ret = send(clientd, buffer2, strlen(buffer2), 0))< 0)
		    {
		      printf("%s ", td.prog);
		      td.status = 400;
		      exit(1);
		    }
		}
	    }
	  fclose(pFile);
	  fclose(head);
	}
      int s = shutdown(clientd, 2);
    }

  else
    { //strange request
      td.status = 501;
      Header(td.status, timestamp, td.count);
      FILE * head;
      head = fopen("header.txt", "r");
      char buffer[100];
      while (!feof(head))
	{
	  if (fgets(buffer, 100, head) != NULL)
	    {
	      if ((ret = send (clientd, buffer, strlen(buffer),0)) < 0)
		{
		  printf("%s ", td.prog);
		  td.status = 400;
		  exit(1);
		}
	    }
	}
    }

  FILE * f12;
  f12 = fopen("log.txt", "a");
  if (f12 ==NULL)
    perror("error opening file");
  else 
    {
      fputs("Request ID: ", f12);
      fprintf(f12, "%d", next_request_id);
      fputs(" ThreadID: ", f12);
      fprintf(f12, "%d",td.id);
      fputs(" ", f12);
      fputs(timestamp, f12);

      fputs("\nRequest ID: ", f12);
      fprintf(f12, "%d ", next_request_id);
      fputs(buff, f12);
      fputs("Request ID: ", f12);
      fprintf(f12, "%d", next_request_id);
      fputs(" HTTP/1.1 ", f12);
      fprintf(f12, "%d\n", td.status);
    }
  fclose(f12);
}




int main(int argc, char **argv) {
  char *prog = argv[0];
  int port;
  int serverd;  /* socket descriptor for receiving new connections */
  int request_init = 0;

  FILE * logfile;
  logfile = fopen("log.txt", "w");
  if (logfile == NULL)
    perror("Error opening log.txt");
  fclose(logfile);


  if (argc < 2) {
    printf("Usage:  %s port\n", prog);
    return 1;
  }
  port = atoi(argv[1]);

  if ((serverd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("%s ", prog);
    perror("socket()");
    return 1;
  }
  
  struct sockaddr_in sa;
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = INADDR_ANY;

  if (bind(serverd, (struct sockaddr*) &sa, sizeof(sa)) < 0) {
    printf("%s ", prog);
    perror("bind()");
    return 1;
  }
  if (listen(serverd, 5) < 0) {
    printf("%s ", prog);
    perror("listen()");
    return 1;
  }

  int n; 
  for (n = 0; n < THREAD_NO; n++)
    {
      tdarray[n].prog = prog;
      tdarray[n].ssockd = serverd;
      tdarray[n].id = n;
    }

  
  int clientd;  /* socket descriptor for communicating with client */
  struct sockaddr_in ca;
  int size = sizeof(struct sockaddr);

  printf("Waiting for a incoming connection...\n");
  if ((clientd = accept(serverd, (struct sockaddr*) &ca, &size)) < 0) {
    printf("%s ", prog);
    perror("accept()");
    return 1;
  }

  char buff[MAXBUFF];  /* message buffer */
  int ret;  /* return value from a call */
  if ((ret = recv(clientd, buff, MAXBUFF-1, 0)) < 0) {
    printf("%s ", prog);
    perror("recv()");
    return 1;
  }

  buff[ret] = '\0';  // add terminating nullbyte to received array of char
  printf("Received message (%d chars):\n%s\n", ret, buff);

  if ((ret = close(clientd)) < 0) {
    printf("%s ", prog);
    perror("close(clientd)");
    return 1;
  }
  if ((ret = close(serverd)) < 0) {
    printf("%s ", prog);
    perror("close(serverd)");
    return 1;
  }
  return 0;
}
