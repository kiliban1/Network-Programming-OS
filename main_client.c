#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <pthread.h>
#include <fcntl.h>

#define PORT_NUM 12345

char user_name[32];

void error(const char *msg)
{
	perror(msg);
	exit(0);
}

typedef struct _ThreadArgs {
	int clisockfd;
} ThreadArgs;

void* thread_main_recv(void* args)
{
	pthread_detach(pthread_self());

	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	// keep receiving and displaying message from server
	char buffer[512];
	int n = 1;

	while (n > 0) {
		memset(buffer, 0, 512);
		n = recv(sockfd, buffer, 512, 0);
		if (n < 0) error("ERROR recv() failed");

		printf("\n%s\n", buffer);
	}

	return NULL;
}

void* thread_main_send(void* args)
{
	int sockfd = ((ThreadArgs*) args)->clisockfd;
	free(args);

	// keep sending messages to the server
	char buffer[256];
	int n;

	if(fcntl(sockfd, F_GETFD) == -1){
		printf("NOT VALID FILE DESCRIPTIOR\n");
		printf("THREAD FILE DES: %d\n", sockfd);
	}

	n = send(sockfd, user_name, strlen(user_name), 0);
	if (n < 0) error("ERROR writing to socket");

	while (1) {
		// You will need a bit of control on your terminal
		// console or GUI to have a nice input window.
		printf("\nPlease enter the message: ");
		memset(buffer, 0, 256);
		fgets(buffer, 255, stdin);

		n = send(sockfd, buffer, strlen(buffer), 0);

		if (n < 0) error("ERROR writing to socket");
		else if (strlen(buffer) < 2) break; // we stop transmission when user type empty string
	}

	
	return NULL;
}

void room_assignment(int sockfd, int new_room, int room_choice){

	char room_options[3];
	char room_prompt[8];
	char buffer[512];
	int n, len;

	// ROOM CREATION
// ---------------------------
// Room_option[0]: 1 = new, 2 = not new room
// Room_option[1]: 1 = no room num, >1 = valid room num (1 is added to all room num)

	memset(room_options, 0, 3);
	memset(buffer, 0, 512);
	room_options[0] = new_room;
	room_options[1] = room_choice;

	if((new_room == 1 && room_choice > 0) || room_choice < 0){
		error("ERROR Unknown room option");
	}

	do{
		n = send(sockfd, room_options, 3, 0);
		if (n < 0) error("ERROR writing to socket");

		memset(buffer, 0, 512);
		n = recv(sockfd, buffer, 512, 0);
		if (n < 0) error("ERROR recv() failed");

		len = strlen(buffer);

		if(len > 1){

			// If returned data len is odd then its corrupt
			if(len & 1)
				error("ROOM OPTIONS RETURNED corrupt!");
			
			
			// Reset options to defaults
			room_options[0] = 2;
			room_options[1] = 0;

			printf("\n");

			if(new_room == 1 && room_choice == 0)
				printf("At max room capacity!\n");
			else if(new_room != 1 && room_choice > 0)
				printf("That room number is not available!\n");

			printf("Server says following options are available\n");

			if(buffer[0] == '!' && buffer[1] != '\0'){
				printf("Rooms 1-%d\n", buffer[1]);
			}
			else{
				for(int i = 0; i + 1 < len; i += 2){
					if(buffer[i + 1] == 1)
						printf("	Room %d: 1 person\n", buffer[i]);
					else
						printf("	Room %d: %d people\n", buffer[i], buffer[i + 1]);
				}
			}

			do{
				if(new_room == 1)
					printf("\nChoose a room number: ");
				else
					printf("\nChoose the room number or type [new] to create a new room: ");

				memset(room_prompt, 0, 8);
				fgets(room_prompt, 7, stdin);

				if(new_room != 1 && (strcmp(room_prompt, "new") == 0 || strcmp(room_prompt, "new\n") == 0)){
					room_options[0] = 1;
				}
				else if(strspn(room_prompt, "0123456789\n") == strlen(room_prompt)){

					n = atoi(room_prompt);

					if(n < 1 || (buffer[0] == '!' && n > buffer[1]))
						printf("Invalid choice\n");	
					else 
						room_options[1] = n;
				}
				else{
					printf("Invalid choice\n");
				}

			}while(room_options[0] != 1 && room_options[1] == 0);

			new_room = room_options[0];
			room_choice = room_options[1];
		}

	}while(len > 1);

	if(new_room == 1)
		printf("Connected to 127.0.0.1 with new room number %d\n", buffer[0]);
	else
		printf("Connected to 127.0.0.1 with room number %d\n", buffer[0]);
	
// ---------------------------

}

int main(int argc, char *argv[])
{
	int new_room = 2;		// 1 - New room, 2 - No new room
	int room_choice = 0; 		// 0 is default no room number

	if (argc < 2) error("Please specify hostname");

	// Check for username
	printf("Enter a username or press enter to use IP address: ");
	memset(user_name, 0, 32);
	fgets(user_name, 31, stdin);

	// Check for new room option and room number
	if (argc > 2) {
		char* curr_arg = argv[2];

		if(strcmp(curr_arg, "new") == 0 || strcmp(curr_arg, "new\n") == 0){
			new_room = 1;
		}
		else if(strspn(curr_arg, "0123456789\n") == strlen(curr_arg)){

			room_choice = atoi(curr_arg);
		}
		else{
			error("INVALID room option");
		}
		
	}

	//printf("New Room: %d    Room #: %d\n", new_room, room_choice);

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) error("ERROR opening socket");

	struct sockaddr_in serv_addr;
	socklen_t slen = sizeof(serv_addr);
	memset((char*) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(PORT_NUM);

	printf("Try connecting to %s...\n", inet_ntoa(serv_addr.sin_addr));

	int status = connect(sockfd, (struct sockaddr *) &serv_addr, slen);
	if (status < 0) error("ERROR connecting");

	room_assignment(sockfd, new_room, room_choice);

	pthread_t tid1;
	pthread_t tid2;

	ThreadArgs* args;
	
	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;

	pthread_create(&tid1, NULL, thread_main_send, (void*) args);

	args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
	args->clisockfd = sockfd;
	pthread_create(&tid2, NULL, thread_main_recv, (void*) args);

	// parent will wait for sender to finish (= user stop sending message and disconnect from server)
	pthread_join(tid1, NULL);

	close(sockfd);

	return 0;
}

