all:
	gcc -o main_server main_server.c -lpthread
	gcc -o main_client main_client.c -lpthread
	gcc -o server server.c -lpthread
	gcc -o client client.c -lpthread
clean:
	rm main_server main_client server client

