
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
#include <unistd.h>

void panic(char *msg);
#define panic(m)	{perror(m); abort();}

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
void* th_led_timer(void* arg);

void get_status(char* status_str);


void led_turn_ON();
void led_turn_OFF();

//variaveis globais
status_e cli_status = ONLINE;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_send = PTHREAD_COND_INITIALIZER; // condition_ellapsed = PTHREAD_COND_INITIALIZER;

pthread_mutex_t mutex_timer = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_timer = PTHREAD_COND_INITIALIZER; // condition_ellapsed = PTHREAD_COND_INITIALIZER;


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
	pthread_create(&th_status_update, NULL, th_led_timer, NULL);

	//iniciar o modulo de driver do led
	system("insmod led.ko");
	

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
			if(scanf("%[^\n]s", pack.buff))
			{
				//pack.type = MESSAGE;	//not needed here (never changes)
				send(sd, &pack, sizeof(pack), 0);
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
			{	//caso nao seja um comando simplemte da print à mensagem
				printf("%s\n", pack.buff);
				led_turn_ON();
				pthread_cond_signal(&condition_timer);	//envia o sinal para que o thread possa apagar o led 1 segundo depois
			}
		}
		else	//o server desconectou-se
		{	
			printf("server down...\n");
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
		time_aux.tv_sec = time(NULL) + 5;	//checks every 10 secons
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

	pthread_exit(NULL);
}


void* th_led_timer(void* arg)
{

	while (1)
	{
		pthread_cond_wait(&condition_timer, &mutex_timer);
		sleep(1);
		led_turn_OFF();
	}

	pthread_exit(NULL);
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


void led_turn_ON()
{
	system("echo 1 >/dev/led0");
}

void led_turn_OFF()
{
	system("echo 0 >/dev/led0");
}

