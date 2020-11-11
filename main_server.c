#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <semaphore.h>

#define PORT_NUM 12345
#define NUM_OF_ROOMS 5

void error(const char *msg)
{
	perror(msg);
	exit(1);
}

typedef struct _USR {
	int clisockfd;		// socket file descriptor
	struct _USR* next;	// for linked list queue
	struct _USR* prev;	// for linked list queue
	char* user_name;
	int room_num;
} USR;

USR *head = NULL;
USR *tail = NULL;

pthread_mutex_t mutex;

int rooms[NUM_OF_ROOMS];

void display_connected(){

	// traverse through all connected clients
	USR* cur = head;

	// figure out sender address
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);

	printf("\n----Connected Clients----\n");

	while (cur != NULL) {

		if (getpeername(cur->clisockfd, (struct sockaddr*)&cliaddr, &clen) < 0) error("ERROR Unknown sender!");

		printf("[%s]\n", inet_ntoa(cliaddr.sin_addr));

		cur = cur->next;
	}

	printf("\n-------Open Rooms-------\n");
	pthread_mutex_lock(&mutex);
	for(int i = 0; i < NUM_OF_ROOMS; i++){

		if(rooms[i] > 0){
			if(rooms[i] == 1)
				printf("Room %d: 1 person\n", i + 1);
			else
				printf("Room %d: %d people\n", i + 1, rooms[i]);
		}
	}
	pthread_mutex_unlock(&mutex);
	printf("-------------------------\n");
}

void add_tail(int newclisockfd)
{
	if (head == NULL) {
		head = (USR*) malloc(sizeof(USR));
		head->clisockfd = newclisockfd;
		head->room_num = -1;
		head->next = head->prev = NULL; 
		tail = head;

		
	} else {
		tail->next = (USR*) malloc(sizeof(USR));
		tail->next->clisockfd = newclisockfd;
		tail->next->room_num = -1;
		tail->next->next = NULL;
		tail->next->prev = tail;
		tail = tail->next;
	}
}

void remove_user(USR* victim)
{
	if(victim->prev != NULL)
		victim->prev->next = victim->next;

	if(victim->next != NULL)
		victim->next->prev = victim->prev;

	if(victim == head)
		head = head->next;
	
	if(victim == tail)
		tail = tail->prev;
	

	// figure out sender address
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);

	if (getpeername(victim->clisockfd, (struct sockaddr*)&cliaddr, &clen) < 0) error("ERROR Unknown sender!");
		
	printf("\n[%s] %s DISCONNECTED!\n", inet_ntoa(cliaddr.sin_addr), victim->user_name);
	
	if(victim->room_num > -1){
		pthread_mutex_lock(&mutex);
		rooms[victim->room_num]--;
		pthread_mutex_unlock(&mutex);
	}

	free(victim);

	display_connected();
}


void broadcast(USR* sendingUsr, char* message)
{
	if(fcntl(sendingUsr->clisockfd, F_GETFD) == -1){
		remove_user(sendingUsr);
		return;
	}

	// figure out sender address
	struct sockaddr_in cliaddr;
	socklen_t clen = sizeof(cliaddr);
	if (getpeername(sendingUsr->clisockfd, (struct sockaddr*)&cliaddr, &clen) < 0) error("ERROR Unknown sender!");

	// traverse through all connected clients
	USR* cur = head;
	while (cur != NULL) {

		// remove users with invalid file descriptors
		if(fcntl(cur->clisockfd, F_GETFD) == -1){
			remove_user(cur);
		}
		// check if cur is not the one who sent the message
		else if (cur->clisockfd != sendingUsr->clisockfd && cur->room_num == sendingUsr->room_num) {
			
			char buffer[512];

			// prepare message
			memset(buffer, 0, 512);

			if(sendingUsr->user_name[0] != '\0')
				sprintf(buffer, "[%s]:%s", sendingUsr->user_name, message);
			else
				sprintf(buffer, "[%s]:%s", inet_ntoa(cliaddr.sin_addr), message);

			int nmsg = strlen(buffer);

			// send!
			int nsen = send(cur->clisockfd, buffer, nmsg, 0);
			if (nsen != nmsg) error("ERROR send() failed");
		}

		cur = cur->next;
	}
}

int room_assignment(int clisockfd){

	char room_options[3];
	char buffer[512];
	int new_room, room_choice;
	int nrcv, nsen, len;

	while(1){

		memset(room_options, 0, 3);
		memset(buffer, 0, 512);
		nrcv = recv(clisockfd, room_options, 3, 0);
		if (nrcv < 0) error("ERROR recv() failed");
		else if(nrcv == 0) return 0;

		new_room = room_options[0];
		room_choice = room_options[1];

		printf("\nRoom options recieved New: %d and Room#: %d\n", new_room, room_choice);
		if(new_room == 1 && room_choice == 0){

			//printf("NEW keyword found for room assign\n");
			pthread_mutex_lock(&mutex);
			for(int i = 0; i < NUM_OF_ROOMS; i++){

				if(rooms[i] == 0){
					
					rooms[i]++;
		
					pthread_mutex_unlock(&mutex);

					buffer[0] = i + 1;

					int nsen = send(clisockfd, buffer, 1, 0);
					if (nsen != 1) error("ERROR send() failed");

					return i;
				}
			}
			pthread_mutex_unlock(&mutex);
			
		}
		else if(new_room != 1 && room_choice > 0){

			//printf("ROOM NUM found for room assign\n");
			
			if((room_choice - 1) < NUM_OF_ROOMS){
				pthread_mutex_lock(&mutex);
				rooms[room_choice - 1]++;

				pthread_mutex_unlock(&mutex);
					
				buffer[0] = room_choice;

				int nsen = send(clisockfd, buffer, 1, 0);
				if (nsen != 1) error("ERROR send() failed");
					
				return (room_choice - 1);
			}

		}

		//printf("Creating room list\n");
		int j = 0;
		
		pthread_mutex_lock(&mutex);
		for(int i = 0; i < NUM_OF_ROOMS; i++){

			
			if(rooms[i] > 0){
				buffer[j] = i + 1;
				buffer[j + 1] = rooms[i];
				j += 2;
			}
			
		}	
		pthread_mutex_unlock(&mutex);
	
		if(j == 0 && room_choice == 0){
			//printf("No rooms, NEW keyword implicit\n");

			pthread_mutex_lock(&mutex);
			rooms[0]++;
			pthread_mutex_unlock(&mutex);

			buffer[0] = 1;

			int nsen = send(clisockfd, buffer, 1, 0);
			if (nsen != 1) error("ERROR send() failed");
			return 0;
		}
		else if(j == 0 && room_choice > 0){

			buffer[0] = '!';
			buffer[1] = NUM_OF_ROOMS;
		}
		

		len = strlen(buffer);

		//printf("Sending room list\n");
		int nsen = send(clisockfd, buffer, len, 0);
		if (nsen != len) error("ERROR send() failed");
	}

}

typedef struct _ThreadArgs {
	int clisockfd;
	USR* user;
} ThreadArgs;

void* thread_main(void* args)
{
	// make sure thread resources are deallocated upon return
	pthread_detach(pthread_self());

	// get socket descriptor from argument
	int clisockfd = ((ThreadArgs*) args)->clisockfd;
	USR* currUsr = ((ThreadArgs*) args)->user;
	free(args);

	//-------------------------------
	// Now, we receive/send messages
	char buffer[256];
	char user_name[32];
	int nsen, nrcv, len;

	currUsr->room_num = room_assignment(clisockfd);

	memset(user_name, 0, 32);
	nrcv = recv(clisockfd, user_name, 32, 0);

	if (nrcv < 0) error("ERROR recv() failed");
	
	len = strlen(user_name);

	if(user_name[len - 1] == '\n') user_name[len - 1] = '\0';

	currUsr->user_name = strdup(user_name);

	while (1) {

		memset(buffer, 0, 256);
		nrcv = recv(clisockfd, buffer, 256, 0);

		if (nrcv < 0) error("ERROR recv() failed");

		len = strlen(buffer);

		if(len < 2) break;
		else if(buffer[len - 1] == '\n') buffer[len - 1] = '\0';

		printf("Message recv from [%s] Room %d: {%s}\n", user_name, (currUsr->room_num + 1), buffer);

		// we send the message to everyone except the sender
		broadcast(currUsr, buffer);
	}

	remove_user(currUsr);

	close(clisockfd);
	//-------------------------------

	return NULL;
}

int main(int argc, char *argv[])
{
	pthread_mutex_init(&mutex, NULL);

	memset(rooms, 0, NUM_OF_ROOMS);

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;	
	//serv_addr.sin_addr.s_addr = inet_addr("192.168.1.171");	
	serv_addr.sin_port = htons(PORT_NUM);

	int status = bind(sockfd, (struct sockaddr*) &serv_addr, slen);
	if (status < 0) error("ERROR on binding");

	listen(sockfd, 5); // maximum number of connections = 5

	while(1) {
		struct sockaddr_in cli_addr;
		socklen_t clen = sizeof(cli_addr);
		int newsockfd = accept(sockfd, 
			(struct sockaddr *) &cli_addr, &clen);
		if (newsockfd < 0) error("ERROR on accept");

		//printf("Connected: %s\n", inet_ntoa(cli_addr.sin_addr));
		add_tail(newsockfd); // add this new client to the client list
		display_connected();

		// prepare ThreadArgs structure to pass client socket
		ThreadArgs* args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
		if (args == NULL) error("ERROR creating thread argument");
		
		args->clisockfd = newsockfd;
		args->user = tail;

		pthread_t tid;
		if (pthread_create(&tid, NULL, thread_main, (void*) args) != 0) error("ERROR creating a new thread");
	}

	pthread_mutex_destroy(&mutex);

	return 0; 
}

