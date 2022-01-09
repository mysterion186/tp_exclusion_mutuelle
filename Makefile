all : serveur client
serveur : serveur.c
	gcc -o serveur serveur.c

client : client.c
	gcc -o client client.c
