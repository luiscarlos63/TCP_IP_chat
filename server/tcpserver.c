#include <sys/socket.h>
#include <sys/types.h>
#include <resolv.h>
#include <pthread.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

void panic(char *msg);
#define panic(m)	{perror(m); abort();}
#define MAX_CLIENTS 10
#define CHECK_TIME 5

//structs
typedef struct client_s{
	int sd_id;
	char name[20];
	pthread_t th_id;
}client_t;

typedef struct message_s
{
	client_t cli;
	char buff[256];
}message_t;


struct node_s
{
    message_t message;
    struct node_s *next;
};
typedef struct node_s node_t;

typedef struct message_queue_s
{
    int count;
    node_t *front;
    node_t *rear;
}message_queue_t;

//prototipos
void *threadfuntion(void *arg);
void* th_sender_fun(void* arg);
void* th_status_checker_fun(void* arg);

void insert_client(client_t cli);
client_t remove_clients(client_t cli);

void send_message_handler(char* buffer, client_t cli);
void initialize(message_queue_t *q);
int isempty(message_queue_t *q);
void enqueue(message_queue_t *q, message_t message);
message_t dequeue(message_queue_t *q);

//variaveis globais
client_t client_arry[MAX_CLIENTS];
message_queue_t message_queue;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER, mutex2 = PTHREAD_MUTEX_INITIALIZER, mutex_time = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition = PTHREAD_COND_INITIALIZER, condition_time = PTHREAD_COND_INITIALIZER;

int main(int count, char *args[])
{	
	struct sockaddr_in serv_addr, cli_addr;
	int listen_sd, port;
	pthread_t th_sender, th_status_checker;

	initialize(&message_queue);


	//inicalizar a lista de clientes
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		client_arry[i].sd_id = 0;
	}
	

	// ------------------------------------------------------------------------------------------
	if ( count != 2 )
	{
		printf("usage: %s <protocol or portnum>\n", args[0]);
		exit(0);
	}

	/*---Get server's IP and standard service connection--*/
	if ( !isdigit(args[1][0]) )
	{
		struct servent *srv = getservbyname(args[1], "tcp");
		if ( srv == NULL )
			panic(args[1]);
		printf("%s: port=%d\n", srv->s_name, ntohs(srv->s_port));
		port = srv->s_port;
	}
	else
		port = htons(atoi(args[1]));

	/*--- create socket ---*/
	listen_sd = socket(PF_INET, SOCK_STREAM, 0);
	if ( listen_sd < 0 )
		panic("socket");

	/*--- bind port/address to socket ---*/
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = port;
	serv_addr.sin_addr.s_addr = INADDR_ANY;                   /* any interface */
	if ( bind(listen_sd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0 )
		panic("bind");
	// ------------------------------------------------------------------------------------------

	pthread_create(&th_sender, NULL, th_sender_fun, NULL);
	pthread_detach(th_sender);                      /* don't track it */
	pthread_create(&th_status_checker, NULL, th_status_checker_fun, NULL);
	pthread_detach(th_status_checker);                      /* don't track it */

	/*--- make into listener with 10 slots ---*/
	if ( listen(listen_sd, 10) != 0 )
		panic("listen")
	else	/*--- begin waiting for connections ---*/
	{	

		

		while (1)                         /* process all incoming clients */
		{
			client_t cli;
			int n = sizeof(cli_addr);
			cli.sd_id = accept(listen_sd, (struct sockaddr*)&cli_addr, &n);     /* accept connection */
			if(cli.sd_id!=-1)
			{
				pthread_t child;
				insert_client(cli);
				printf("New connection\n");
				pthread_create(&child, 0, threadfuntion, &cli);       /* start thread */
				pthread_detach(child);                      /* don't track it */
			}
		}
	}
}	//fim da main




void *threadfuntion(void *arg)                    
{	
	client_t cli = *(client_t*)arg;            /* get & convert the socket */
	char buffer[256];
	int error_flag = 0;
	socklen_t len = sizeof (error_flag);

	recv(cli.sd_id,buffer,sizeof(buffer),0);
	strcpy(cli.name, buffer);
	printf("%s: ola meus putos\n", cli.name);
	
	strcpy(buffer, " juntou-se ao chat\n");
	send_message_handler(buffer, cli);				//esta funçao ira mandar a 

	while (1)
	{

		if(recv(cli.sd_id,buffer,sizeof(buffer),0))
		{
			send_message_handler(buffer, cli);
		}
		else
		{
			break;
		}
	}
	
	remove_clients(cli);

	printf("o cliente: %s saiu do chat", cli.name);
	//send(cli.sd_id,buffer,sizeof(buffer),0);

	shutdown(cli.sd_id,SHUT_RD);
	shutdown(cli.sd_id,SHUT_WR);
	shutdown(cli.sd_id,SHUT_RDWR);
	// 		                  /* close the client's channel */
	pthread_exit(NULL);
	//return 0;                           /* terminate the thread */
}


void* th_sender_fun(void* arg)
{
	message_t message;
	char buff[256];		

	while(1)
	{
		pthread_mutex_lock(&mutex);
		if(!isempty(&message_queue))
		{
			message = dequeue(&message_queue);
			if(message.cli.sd_id != -1)
			{
				strcpy(buff, message.cli.name); //copia para o buffer o nome do cliente que esta a mandar a mensagem
				strcat(buff, ": ");
				strcat(buff, message.buff);

				for (int i = 0; i < MAX_CLIENTS; i++)
				{
					/* Envia para todos os clientes menos para o proprio
						Tambem verifica se o cliente existe id =/= 0
					*/
					if((client_arry[i].sd_id != message.cli.sd_id) && (client_arry[i].sd_id != 0))
					{
						send(client_arry[i].sd_id, buff, sizeof(buff), 0);
					}
				}
			}
		}
		else
		{
			//waits ultil a signal	
			pthread_cond_wait(&condition, &mutex);
		}
		pthread_mutex_unlock(&mutex);
	}


}


/*FUNÇAO AINDA INACABADA*/
void* th_status_checker_fun(void* arg)
{
	int i = 0;
	struct timespec sleep_until;
	
	while (1)
	{
		pthread_mutex_lock(&mutex_time);
		sleep_until.tv_sec = (time(NULL) + CHECK_TIME);		//CHECK_TIME represena 5 segundos
		sleep_until.tv_nsec = 0;

		// for(i = 0; i < MAX_CLIENTS; i++)
		// {
		// 	if(client_arry[i].sd_id != 0)
		// 	{
		// 		check_cli_status(client_arry[i]);
		// 	}
		// }	
		
		pthread_cond_timedwait(&condition_time, &mutex_time, &sleep_until);
		pthread_mutex_unlock(&mutex_time);
	}
}



// +++++++++++++++++++++++++++++++++++++++++ FUNÇOES PARA METER E TIRAR clientes da lista +++++++++++++++++++++++
void insert_client(client_t cli)
{
	int i = 0;
	for(i = 0; i < MAX_CLIENTS; i++)
	{
		if(client_arry[i].sd_id == 0)
		{
			client_arry[i] = cli;
			printf("client inserted: %d\n", cli.sd_id);
			break;
		}
	}
	if(i >= MAX_CLIENTS)
	{
		printf("erro: a lista esta cheia\n");
	}
}

client_t remove_clients(client_t cli)
{
	int i = 0;
	for(i = 0; i < MAX_CLIENTS; i++)
	{
		if(client_arry[i].sd_id == cli.sd_id)
		{
			client_arry[i].sd_id = 0;
			return cli;
		}
	}
}


//--------------------------------	FUNÇOES para meter a mensagem numa "queqe" e MAIS ----------------------
void send_message_handler(char* buffer, client_t cli)
{
	message_t message;
	
	strcpy(message.buff, buffer);
	message.cli = cli;

	enqueue(&message_queue, message);
	pthread_cond_signal(&condition);
}

void initialize(message_queue_t *q)
{
    q->count = 0;
    q->front = NULL;
    q->rear = NULL;
}

int isempty(message_queue_t *q)
{
    return (q->front == NULL);
}

void enqueue(message_queue_t *q, message_t message)
{
	pthread_mutex_lock(&mutex);
		if (q->count < MAX_CLIENTS)
		{
			node_t *tmp;
			tmp = malloc(sizeof(node_t));
			tmp->message = message;
			tmp->next = NULL;
			if(!isempty(q))
			{
				q->rear->next = tmp;
				q->rear= tmp;
			}
			else
			{
				q->front = q->rear = tmp;
			}
			q->count++;
		}
		else
		{
			printf("List is full\n");
		}
	pthread_mutex_unlock(&mutex);
}


message_t dequeue(message_queue_t *q)
{
    node_t *tmp;
	message_t message;
	message.cli.sd_id = -1;
	
	pthread_mutex_lock(&mutex2);
	if(q->front == NULL)
	{
		q->front = q->rear = NULL;
	}
	else
	{
		message = q->front->message;
		tmp = q->front;
		q->front = q->front->next;
		q->count--;
		free(tmp);
	}
	pthread_mutex_unlock(&mutex2);
    
	return(message);
}



