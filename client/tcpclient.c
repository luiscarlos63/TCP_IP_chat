
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
#include <mqueue.h>	

void panic(char *msg);
#define panic(m)	{perror(m); abort();}

/* name of the POSIX object referencing the queue */
#define MSGQOBJ_NAME    "/tcp_message"
/* max length of a message (just for this process) */
#define MAX_MSG_LEN     100000



typedef enum status
{
	ONLINE,
	AFK
}status_e;

typedef enum stream_type
{
	MESSAGE,
	COMMAND
}stream_type_e;

typedef struct package_s
{
	stream_type_e type;
	char buff[256];
}package_t;

//prototipos
void* th_receiver_func(void* arg);
void* th_status_update_func(void* arg);
void get_status(char* status_str);

//variaveis globais
status_e cli_status = ONLINE;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_send = PTHREAD_COND_INITIALIZER; // condition_ellapsed = PTHREAD_COND_INITIALIZER;


mqd_t msgq_id;
int sd;

/****************************************************************************/
/*** This program opens a connection to a server using either a port or a ***/
/*** service.  Once open, it sends the message from the command line.     ***/
/*** some protocols (like HTTP) require a couple newlines at the end of   ***/
/*** the message.                                                         ***/
/*** Compile and try 'tcpclient lwn.net http "GET / HTTP/1.0" '.          ***/
/****************************************************************************/

static void int_handler(int signo){		
	if(signo==SIGINT){	/* handler of SIGNAL*/
		shutdown(sd,SHUT_RD);
		shutdown(sd,SHUT_WR);
		shutdown(sd,SHUT_RDWR);
		mq_close(msgq_id); /*Close queue*/
		exit(1);
		
	}
}

int main(int count, char *args[])
{	struct hostent* host;
	struct sockaddr_in addr;
	int port;
	char name[20];
	pthread_t th_receiver, th_status_update;
	struct timespec time_last_send;
	
	//message queue variables
	
	unsigned int mess_que_prio;
	//char queue_message_buff[MAX_MSG_LEN];
	int mq_recv_ret;
	
	signal(SIGINT,  int_handler);

	
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

	//copys the name apssed by armument and stores it
	strcpy(name, args[3]);

	//thread to the status update
	pthread_create(&th_status_update, NULL, th_status_update_func, &time_last_send);

	//message queue
	/* opening the queue using default attributes  --  mq_open() */
  	msgq_id = mq_open(MSGQOBJ_NAME, O_RDWR | O_CREAT , S_IRWXU | S_IRWXG, NULL);
	if (msgq_id == (mqd_t)-1) {
		perror("In mq_open()");
		exit(1);
	}


	/*---If connection successful, send the message and read results---*/
	if ( connect(sd, (struct sockaddr*)&addr, sizeof(addr)) == 0)
	{	
		
        //char buffer[256];
		package_t pack;

		pack.type = MESSAGE;	//not needed here but if done here doesn't need to be done again in the loop...
		strcpy(pack.buff, name);
		send(sd, &pack, sizeof(pack),0);                     /* send name to server*/

		pthread_create(&th_receiver, NULL, th_receiver_func, &sd);		

		while(1)
		{
			// if(scanf("%[^\n]s", pack.buff))
			// {
			// 	//pack.type = MESSAGE;	//not needed here (never changes)
			// 	send(sd, &pack, sizeof(pack), 0);
			// 	cli_status = ONLINE;	
				
			// 	time_last_send.tv_sec = time(NULL);		//Regista o tempo em que mandou a ultima mensagem

			// 	pthread_cond_signal(&condition_send);	
				
			// 	while((getchar()) != '\n');	//reset para o scanf
			// }

			mq_recv_ret = mq_receive(msgq_id, pack.buff, MAX_MSG_LEN, NULL);
			if (mq_recv_ret == -1) {
				perror("In mq_receive()");
				exit(1);
    		}
			if(mq_recv_ret != -1)
			{
				pack.type = MESSAGE;	//not needed here (never changes)
				send(sd, &pack, sizeof(pack), 0);
				cli_status = ONLINE;	
					
				time_last_send.tv_sec = time(NULL);		//Regista o tempo em que mandou a ultima mensagem

				pthread_cond_signal(&condition_send);	
			}
		}
	}
	else
		panic("connect");
}



void* th_receiver_func(void* arg)
{
	int sd = *((int*)arg);
	//char buffer[256];
	package_t pack;


	while(1)
	{
		if(recv(sd, &pack, sizeof(pack), 0))
		{
			//como neste momento so existe este comando nao sera necesario um parsing mais complexo()
			if(pack.type == COMMAND)		
			{
				char status_str[20];
				get_status(status_str);			//junta tudo na mesma mensagem
				strcat(pack.buff, status_str);	
				send(sd, &pack, sizeof(pack), 0);	//manda
			}	
			else	
			{		//caso nao seja um comando simplemte da print à mensagem
				printf("%s\n", pack.buff);
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

