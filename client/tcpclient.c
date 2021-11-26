
/*****************************************************************************/
/*** tcpclient.c                                                           ***/
/***                                                                       ***/
/*** Demonstrate an TCP client.                                            ***/
/*****************************************************************************/

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <resolv.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <time.h>

void panic(char *msg);
#define panic(m)	{perror(m); abort();}

typedef enum status
{
	ONLINE,
	AFK
}status_e;

//prototipos
void* th_receiver_func(void* arg);
void* th_status_update_func(void* arg);
void get_status(char* status_str);

//variaveis globais
status_e cli_status = ONLINE;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_send = PTHREAD_COND_INITIALIZER; // condition_ellapsed = PTHREAD_COND_INITIALIZER;



/****************************************************************************/
/*** This program opens a connection to a server using either a port or a ***/
/*** service.  Once open, it sends the message from the command line.     ***/
/*** some protocols (like HTTP) require a couple newlines at the end of   ***/
/*** the message.                                                         ***/
/*** Compile and try 'tcpclient lwn.net http "GET / HTTP/1.0" '.          ***/
/****************************************************************************/
int main(int count, char *args[])
{	struct hostent* host;
	struct sockaddr_in addr;
	int sd, port;
	char name[20];
	pthread_t th_receiver, th_status_update;
	struct timespec time_last_send;

	

	if ( count != 4 )
	{
		printf("usage: %s <servername> <protocol or portnum>\n", args[0]);
		exit(0);
	}

	/*---Get server's IP and standard service connection--*/
	host = gethostbyname(args[1]);
	//printf("Server %s has IP address = %s\n", args[1],inet_ntoa(*(long*)host->h_addr_list[0]));
	if ( !isdigit(args[2][0]) )
	{
		struct servent *srv = getservbyname(args[2], "tcp");
		if ( srv == NULL )
			panic(args[2]);
		printf("%s: port=%d\n", srv->s_name, ntohs(srv->s_port));
		port = srv->s_port;
	}
	else
		port = htons(atoi(args[2]));

	/*---Create socket and connect to server---*/
	sd = socket(PF_INET, SOCK_STREAM, 0);        /* create socket */
	if ( sd < 0 )
		panic("socket");
	memset(&addr, 0, sizeof(addr));       /* create & zero struct */
	addr.sin_family = AF_INET;        /* select internet protocol */
	addr.sin_port = port;                       /* set the port # */
	addr.sin_addr.s_addr = *(long*)(host->h_addr_list[0]);  /* set the addr */

	strcpy(name, args[3]);


	pthread_create(&th_status_update, NULL, th_status_update_func, &time_last_send);

	/*---If connection successful, send the message and read results---*/
	if ( connect(sd, (struct sockaddr*)&addr, sizeof(addr)) == 0)
	{	
        char buffer[256];

		send(sd, name,sizeof(name),0);                     /* send name to server*/

		pthread_create(&th_receiver, NULL, th_receiver_func, &sd);		

		while(1)
		{
			if(scanf("%[^\n]s", buffer))
			{
				send(sd, buffer, strlen(buffer) + 1, 0);
				cli_status = ONLINE;	
				
				time_last_send.tv_sec = time(NULL);		//Registao tempo em que mandou a ultima mensagem

				pthread_cond_signal(&condition_send);	
				
				while((getchar()) != '\n');	//reset para o scanf
			}
		}
	}
	else
		panic("connect");
}



void* th_receiver_func(void* arg)
{
	int sd = *((int*)arg);
	char buffer[256];

	while(1)
	{
		if(recv(sd, buffer, sizeof(buffer), 0))
		{
			//como neste momento so existe este comando nao sera necesario em parsing mais complexo()
			if(strcmp(buffer, "!status") == 0)		
			{
				char status_str[20];
				get_status(status_str);			//junta tudo na mesma mensagem
				strcat(buffer, status_str);	
				send(sd, buffer, sizeof(buffer), 0);	//manda
			}	
			else	
			{		//caso nao seja um comando simplemte da print à mensagem
				printf("%s\n", buffer);
			}
		}
		else	//o server desconectou-se
		{	
			break;
		}	
	}
	pthread_exit(NULL);
}



void* th_status_update_func(void* arg)
{
	struct timespec* time_last_send = ((struct timespec*)arg);
	struct timespec time_aux;

	while (1)
	{
		time_aux.tv_sec = time(NULL) + 10;	//checks every 10 secons
		time_aux.tv_nsec = 0;
		
		pthread_cond_timedwait(&condition_send, &mutex, &time_aux);
		{
			if(time_aux.tv_sec >= time_last_send->tv_sec + 60) 
			{
					//posso por aqui outro condition varibel que para este processo ate efetivamente
					//	ocorrer outro "send" e com isto aumentar a resoluçao do check
				cli_status = AFK;
			}
		}		
	}
}


void get_status(char* status_str)
{
	if(cli_status == AFK)
	{
		strcpy(status_str, " AFK");
	}
	else if(cli_status == ONLINE)
		 {
			strcpy(status_str, " ONLINE");
		 }
}

