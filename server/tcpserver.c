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


//structs and enums
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

typedef struct client_s{
	int sd_id;
	char name[20];
	status_e status;
	pthread_t th_id;
}client_t;


typedef struct package_s
{
	stream_type_e type;
	char buff[256];
}package_t;



typedef struct message_s
{
	client_t cli;
	package_t package;
}message_t;


typedef struct node_s
{
    message_t message;
    struct node_s *next;
}node_t;


typedef struct message_queue_s
{
    int count;
    node_t *front;
    node_t *rear;
}message_queue_t;


//prototipos
void *th_cli_read_fun(void *arg);
void* th_sender_fun(void* arg);
void* th_status_checker_fun(void* arg);

void insert_client(client_t cli);
client_t remove_clients(client_t cli);
int client_exits(client_t cli);
void client_status_update(const client_t* cli);

void send_message_handler(const message_t* message);
void initialize(message_queue_t *q);
int isempty(message_queue_t *q);
void enqueue(message_queue_t *q, const message_t* message);
message_t dequeue(message_queue_t *q);

void client_status_request(client_t cli);
status_e get_cli_status(char* status_str);


//variaveis globais hehe
client_t client_arry[MAX_CLIENTS];
message_queue_t message_queue;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER, 
				mutex2 = PTHREAD_MUTEX_INITIALIZER, 
				mutex_time = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t condition = PTHREAD_COND_INITIALIZER, 
				condition_time = PTHREAD_COND_INITIALIZER;

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
		client_arry[i].status = ONLINE;
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
			int sd;
			int n = sizeof(cli_addr);
			sd = accept(listen_sd, (struct sockaddr*)&cli_addr, &n);     /* accept connection */
			if(sd!=-1)
			{
				pthread_t child;
				printf("New connection\n");
				pthread_create(&child, 0, th_cli_read_fun, &sd);       /* start thread */
				pthread_detach(child);                      /* don't track it */
			}
		}
	}
}	//fim da main




void *th_cli_read_fun(void *arg)
{	
//	client_t cli;            /* get & convert the socket */
//	package_t package;
	message_t message;

	//int error_flag = 0;

	message.cli.sd_id = *(int*)arg;

	//receive the name of the client
	recv(message.cli.sd_id, &message.package, sizeof(message.package), 0);
	strcpy(message.cli.name, message.package.buff);

	insert_client(message.cli);		//insere o cliente na lista

	//esta funçao ira anunciar que um cliente chegou
	strcpy(message.package.buff, " juntou-se ao chat\n");
	message.package.type = MESSAGE;
	send_message_handler(&message);				
	
	
	while (1)
	{
		if(recv(message.cli.sd_id, &message.package, sizeof(message.package), 0))
		{
			if(message.package.type == COMMAND)	//verifica se é comando 
			{
				//message.cli.status = get_cli_status(buffer);
				client_status_update(&message.cli);			//(neste momento so existe um comando)
			}
			else
			{
				message.package.type = MESSAGE;
				send_message_handler(&message);
			}
		}
		else
		{	
			strcpy(message.package.buff, "desconectou-se...");
			message.package.type = MESSAGE;
			send_message_handler(&message);
			break;
		}
	}
	
	remove_clients(message.cli);

	printf("o cliente: %s saiu do chat", message.cli.name);

	/* close the client's channel */
	shutdown(message.cli.sd_id,SHUT_RD);
	shutdown(message.cli.sd_id,SHUT_WR);
	shutdown(message.cli.sd_id,SHUT_RDWR);
		                  
	pthread_exit(NULL);
	//return 0;                           /* terminate the thread */
}


void* th_sender_fun(void* arg)
{
	message_t message;
	package_t aux_pack;		

	while(1)
	{
		pthread_mutex_lock(&mutex);
		if(!isempty(&message_queue))
		{
			message = dequeue(&message_queue);
			if(message.cli.sd_id != -1)
			{
				aux_pack.type = MESSAGE;
				strcpy(aux_pack.buff, message.cli.name); //copia para o buffer o nome do cliente que esta a mandar a mensagem
				strcat(aux_pack.buff, ": ");
				strcat(aux_pack.buff, message.package.buff);

				for (int i = 0; i < MAX_CLIENTS; i++)
				{
					/* Envia para todos os clientes menos para o proprio
						Tambem verifica se o cliente existe (id =/= 0)
					*/
					if((client_arry[i].sd_id != message.cli.sd_id) && (client_arry[i].sd_id != 0))
					{
						send(client_arry[i].sd_id, &aux_pack, sizeof(aux_pack), 0);
					}
				}
			}
		}
		else
		{
			//waits until a signal	
			pthread_cond_wait(&condition, &mutex);
		}
		pthread_mutex_unlock(&mutex);
	}


}


void* th_status_checker_fun(void* arg)
{
	int i = 0;
	struct timespec sleep_until;
	
	while (1)
	{
		pthread_mutex_lock(&mutex_time);
		sleep_until.tv_sec = (time(NULL) + CHECK_TIME);		//CHECK_TIME represena 5 segundos
		sleep_until.tv_nsec = 0;

		for(i = 0; i < MAX_CLIENTS; i++)
		{
			if(client_arry[i].sd_id != 0)
			{
				client_status_request(client_arry[i]);
			}
		}	
		
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

int client_exits(client_t cli)
{
	int i = 0;
	int flag = -1;

	for(i = 0; i < MAX_CLIENTS; i++)
	{
		if(cli.sd_id == client_arry[i].sd_id)
		{
			flag = 1;
			break;
		}
	}
	return flag;
}

void client_status_update(const client_t* cli)
{
	int i = 0;

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if(cli->sd_id == client_arry[i].sd_id)
		{
			client_arry[i].status = cli->status;
		}
	}
}


//--------------------------------	FUNÇOES para meter a mensagem numa "queqe" e MAIS ----------------------
void send_message_handler(const message_t* message)
{
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

void enqueue(message_queue_t *q, const message_t* message)
{
	pthread_mutex_lock(&mutex);
		if (q->count < MAX_CLIENTS)
		{
			node_t *tmp;
			tmp = malloc(sizeof(node_t));

			//copy paremeters "by hand" as both are pointers 
			tmp->message.cli = message->cli;
			tmp->message.package = message->package;
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


// ---------------------------------------	FUNÇAO QUE MANDA O STATUS REQUEST PARA O CLIENTE --------------------------
// isto é feito atraves de enviar uma mensagem para esse cliente atraves da QUEUE 
//	com um carater especial para identificar que se trata de um comando  e nao de uma mensagem -------------------  
void client_status_request(client_t cli)
{
	package_t pack;
		
	if(client_exits(cli))
	{
		pack.type = COMMAND;
		strcpy(pack.buff, "!status");	// '!' funciona como caracter de identificaçao de comando (tipo bot do discord)
		send(cli.sd_id, &pack, sizeof(pack), 0);	//manda para o cliente, (bypasses the MESSAGE QUEUE)
	}
}

status_e get_cli_status(char* status_str)
{

	if(strcmp(status_str, "!status AFK") == 0)
	{
		return AFK;
	}
	if(strcmp(status_str, "!status ONLINE") == 0)
	{
		return ONLINE;
	}
}
