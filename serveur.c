/* T. Grandpierre - Application distribue'e pour TP IF4-DIST 2004-2005

But : 

fournir un squelette d'application capable de recevoir des messages en 
mode non bloquant provenant de sites connus. L'objectif est de fournir
une base pour implementer les horloges logique/vectorielle/scalaire, ou
bien pour implementer l'algorithme d'exclusion mutuelle distribue'

Syntaxe :
         arg 1 : Numero du 1er port
	 arg 2 et suivant : nom de chaque machine

--------------------------------
Exemple pour 3 site :

Dans 3 shells lances sur 3 machines executer la meme application:

pc5201a>./dist 5000 pc5201a.esiee.fr pc5201b.esiee.fr pc5201c.esiee.fr
pc5201b>./dist 5000 pc5201a.esiee.fr pc5201b.esiee.fr pc5201c.esiee.fr
pc5201c>./dist 5000 pc5201a.esiee.fr pc5201b.esiee.fr pc5201c.esiee.fr

pc5201a commence par attendre que les autres applications (sur autres
sites) soient lance's

Chaque autre site (pc5201b, pc5201c) attend que le 1er site de la
liste (pc5201a) envoi un message indiquant que tous les sites sont lance's


Chaque Site passe ensuite en attente de connexion non bloquante (connect)
sur son port d'ecoute (respectivement 5000, 5001, 5002).
On fournit ensuite un exemple permettant 
1) d'accepter la connexion 
2) lire le message envoye' sur cette socket
3) il est alors possible de renvoyer un message a l'envoyeur ou autre si
necessaire 

*/


#include <stdio.h>
#include<stdlib.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<fcntl.h>
#include<netdb.h>
#include<string.h>
#include<time.h>

int GetSitePos(int Nbsites, char *argv[]) ;
void WaitSync(int socket);
void SendSync(char *site, int Port);
// cette fonction aura pour but d'envoyer un message à tous les sites
void envoie_msg(int my_position,int NSites, int * tableau_socket, struct sockaddr_in * tableau_sockaddr,int HL, int intention, int to);
// fonction qui renvoie 1 si tous les sites ont donnés leurs accords pour passer dans la SC 
int accord_tous(int* tableau_attente,int NSites); 
// fonction qui renvoie l'indice de la valeur minimal du tableau (autre que -1)
int min_tableau(int* tableau_attente,int NSites);  


// structure message qu'on va utiliser pour communiquer entre les sites
struct Message {
  int id; // id du site qui envoie le message
  int hl; // valeur de l'horloge au moment de l'envoie des messages
  int intention; // le but de ce msg entrer en section critique, sortie, donne l'autorisation
  int to; // id du site vers qui on envoie un message (on met -1 si c'est un message pour tous le monde)
};
typedef struct Message Message;

// fonction pour obtenir le max entre 2 valeurs
int max(int num1, int num2)
{
    return (num1 > num2 ) ? num1 : num2;
}

/*Identification de ma position dans la liste */
int GetSitePos(int NbSites, char *argv[]) {
  char MySiteName[20]; 
  int MySitePos=-1;
  int i;
  gethostname(MySiteName, 20);
  for (i=0;i<NbSites;i++) 
    if (strcmp(MySiteName,argv[i+1])==0) {
      MySitePos=i;
      //printf("L'indice de %s est %d\n",MySiteName,MySitePos);
      return MySitePos;
    }
  if (MySitePos == -1) {
    printf("Indice du Site courant non trouve' dans la liste\n");
    exit(-1);
  }
  return (-1);
}


/*Attente bloquante d'un msg de synchro sur la socket donne'e*/
void WaitSync(int s_ecoute) {
  char texte[40];
  int l;
  int s_service;
  struct sockaddr_in sock_add_dist;
  socklen_t size_sock;
  size_sock=sizeof(struct sockaddr_in);
  printf("WaitSync : ");fflush(0);
  s_service=accept(s_ecoute,(struct sockaddr*) &sock_add_dist,&size_sock);
  l=read(s_service,texte,39);
  texte[l] ='\0';
  printf("%s\n",texte); fflush(0);
  close (s_service);
} 

/*Envoie d'un msg de synchro a la machine Site/Port*/
void SendSync(char *Site, int Port) {
  struct hostent* hp;
  int s_emis;
  char chaine[15];
  int longtxt;
  struct sockaddr_in sock_add_emis;
  int size_sock;
  int l;
  
  if ( (s_emis=socket(AF_INET, SOCK_STREAM,0))==-1) {
    perror("SendSync : Creation socket");
    exit(-1);
  }
    
  hp = gethostbyname(Site);
  if (hp == NULL) {
    perror("SendSync: Gethostbyname");
    exit(-1);
  }

  size_sock=sizeof(struct sockaddr_in);
  sock_add_emis.sin_family = AF_INET;
  sock_add_emis.sin_port = htons(Port);
  memcpy(&sock_add_emis.sin_addr.s_addr, hp->h_addr, hp->h_length);
  
  if (connect(s_emis, (struct sockaddr*) &sock_add_emis,size_sock)==-1) {
    perror("SendSync : Connect");
    exit(-1);
  }
     
  sprintf(chaine,"**SYNCHRO**");
  longtxt =strlen(chaine);
  /*Emission d'un message de synchro*/
  l=write(s_emis,chaine,longtxt);
  close (s_emis); 
}

// cette fonction aura pour but d'envoyer un message à tous les sites
void envoie_msg(int my_position,int NSites, int * tableau_socket, struct sockaddr_in * tableau_sockaddr,int HL, int intention, int to){
  Message msg;
  int size_sock = sizeof(struct sockaddr_in);
  for (int i = 0; i <NSites;i++){
    // on boucle sur tous les sites sauf le site actuel (pour ne pas s'envoyer un message à soi-même)
    if (i != my_position){
      // création des sockets
      if ((tableau_socket[i] = socket(AF_INET, SOCK_STREAM,0))==-1){
        perror("Création socket");
        exit(-1);
      }
      // connection des sockets au struct sockaddr_in pour pouvoir communiquer
      if (connect(tableau_socket[i],(struct sockaddr*) &tableau_sockaddr[i],size_sock)==-1){
            perror("connect inter-site");
            exit(-1);
        }
      else{ // cas où il n'y a pas d'erreur, on écrit, envoie le message et on ferme la socket 
        msg.id = my_position;
        msg.hl = HL;
        msg.intention = intention;
        msg.to = to;
        write(tableau_socket[i],&msg,sizeof(msg));
        close(tableau_socket[i]);
      }
    }
  }
}

// fonction qui renvoie 1 si tous les sites ont donnés leurs accords pour passer dans la SC 
int accord_tous(int* tableau_attente, int NSites){ 
  for (int i = 0; i <NSites;i++){
    if (tableau_attente[i] != 1){
      return -1;
    }
  }
  return 1;
}

// fonction qui renvoie l'indice de la valeur minimal du tableau (autre que -1)
int min_tableau (int* tableau_attente, int NSites){
  int pos = 0; 
  int min = tableau_attente[0];
  for (int i = 1; i < NSites;i++){
    if (tableau_attente[i]> -1 ){
      if (min != -1 && min>tableau_attente[i] ){
        pos = i;
        min = tableau_attente[i];
      }
      else if (min == -1) {
        pos = i;
        min = tableau_attente[i];
      }
    }
  }
  // s'il y a que des -1 dans le tableau on retourne -1
  if (pos==0 && tableau_attente[0]==-1 ||  tableau_attente[pos]==-1){
    return -1;
  }
  return pos;
}
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/

int main (int argc, char* argv[]) {
  struct sockaddr_in sock_add, sock_add_dist;
  
  int s_ecoute, s_service;
  char texte[40];
  Message msg;
  int i,l,my_position;
  float t;


  int PortBase=-1; /*Numero du port de la socket a` creer*/
  int NSites=-1; /*Nb total de sites*/


  if (argc!=4) {
    printf("Il faut donner le numero de port du site et le nombre total de sites");
    exit(-1);
  }

  /*----Nombre de sites (adresses de machines)---- */
  //NSites=argc-2;
  NSites=atoi(argv[3]); //-2;



  /*CREATION&BINDING DE LA SOCKET DE CE SITE*/
  PortBase=atoi(argv[2]) ;//+GetSitePos(NSites, argv);
  int main_site = atoi(argv[1]);
  printf("Numero de port de ce site %d\n",PortBase);
  my_position = PortBase-main_site; // position du site selon le site de base (correspond à l'identifiant du site)


  sock_add.sin_family = AF_INET;
  sock_add.sin_addr.s_addr= htons(INADDR_ANY);  
  sock_add.sin_port = htons(PortBase);

  /* Création des variables pour réaliser l'algo de Lamport */ 
  struct hostent* hp;
  socklen_t size_sock;
  int size_struct;
  int HL = 0; // va stocker la valeur de l'horloge
  int in_SC = 0; // valeur pour savoir si le processus est en SC 
  int temp = 0; // variable pour arrêter la boucle infinie au bout d'un certaint temps
  int time_SC = 0; // variable qu'on va incrémenter et utiliser pour mettre fin à une SC 

  
  struct sockaddr_in tableau_sockaddr[NSites]; // tableau qui va contenir des struct sockaddr_in pour chaque site
  int tableau_socket[NSites]; // tableau qu'on va utiliser pour intialiser un socket par site
  int tableau_attente[NSites]; // tableau qu'on va utiliser pour voir les processus qui sont dans la file d'attente pour rentrer dans la SC
  int tableau_accord[NSites]; // tableau qu'on va utiliser pour savoir quels processus ont donnés leurs accords pour entrer dans la SC
  
  // initialisation des tableaux 
  for (int i = 0; i < NSites; i++){
    tableau_attente[i] = -1; 
    tableau_accord[i] = -1;
  }
  /*
    Communication entre site
  */
 // Recup adresse tous les sites sont en localhost donc pas de soucis
  hp = gethostbyname("localhost");
  if (hp == NULL) {
		perror("client");
		return -1;
  }

  size_struct = sizeof(struct sockaddr_in);
  // on renseigne les différentes valeurs dans le tableau tableau_sockaddr pour pouvoir communiquer via les sites grâce aux sockets
  for (int i = 0; i < NSites; i++) {
    tableau_sockaddr[i].sin_family = AF_INET;
    tableau_sockaddr[i].sin_port = htons(main_site+i);
    memcpy(&tableau_sockaddr[i].sin_addr.s_addr,hp->h_addr, hp->h_length);
  }


  if ( (s_ecoute=socket(AF_INET, SOCK_STREAM,0))==-1) {
    perror("Creation socket");
    exit(-1);
  }

  if ( bind(s_ecoute,(struct sockaddr*) &sock_add, \
	    sizeof(struct sockaddr_in))==-1) {
    perror("Bind socket");
    exit(-1);
  }
  
  listen(s_ecoute,30);
  /*----La socket est maintenant cre'e'e, binde'e et listen----*/

  if (strcmp(argv[1], argv[2])==0) { 
    /*Le site 0 attend une connexion de chaque site : */
    for(i=0;i<NSites-1;i++) 
      WaitSync(s_ecoute);
    printf("Site 0 : toutes les synchros des autres sites recues \n");fflush(0);
    /*et envoie un msg a chaque autre site pour les synchroniser */
    for(i=0;i<NSites-1;i++) 
      SendSync("localhost", atoi(argv[1])+i+1);
    } else {
      /* Chaque autre site envoie un message au site0 
	 (1er  dans la liste) pour dire qu'il est lance'*/
      SendSync("localhost", atoi(argv[1]));
      /*et attend un message du Site 0 envoye' quand tous seront lance's*/
      printf("Wait Synchro du Site 0\n");fflush(0);
      WaitSync(s_ecoute);
      printf("Synchro recue de  Site 0\n");fflush(0);
  }
  

  // affichage de l'identifiant du site et des numéros de ports des autres sites et leurs identifiants
  printf("\n\nJe suis le site %d sur le port %d\n",my_position,PortBase);
  for (int i = 0; i <NSites;i++){
    if (i != my_position){
      printf("Les autres sites sont %d avec pour identifiant %d\n",main_site+i,i);
    }
  }

  
  /* Passage en mode non bloquant du accept pour tous*/
  /*---------------------------------------*/
  fcntl(s_ecoute,F_SETFL,O_NONBLOCK);
  size_sock=sizeof(struct sockaddr_in);
  
  // seed pour générer les valeurs aléatoires, on met l'identifiant du site en seed pour que chaque site génère des nombres différents
  srand(my_position);
  //srand(time(NULL));
  /* Boucle infini*/
  while(1) {
    temp++; 
    
    if (temp == 30) break;
    int r = rand() % 100 + 1;;
    printf("r : %d\n",r);
    if (r <= 70){
      // on ne fait rien 
    }
    // si 70<r<90 et que le site n'est pas en section critique on incrémente la valeur de l'horloge
    else if (70<r && r<90 && in_SC != 1){
      HL++; 
    }
    // on veut envoyer une demande pour entrer dans la section critique
    else{
      // on vérifie qu'on a pas déjà fait une demande (en regardant si on a pas déjà donné son propre accord pour entrer en SC)
      if (tableau_attente[my_position]==-1){ 
        HL++;
        tableau_attente[my_position]=HL; // on met sa valeur d'horloge dans la file d'attente
        tableau_accord[my_position] = 1; // on indique qu'on donne son propre accord pour entrer en SC
        // envoyer un message à tous les autres sites pour faire une requête d'entrée en SC
        envoie_msg(my_position,NSites,tableau_socket,tableau_sockaddr,HL,1,-1);
        printf("Envoie du message id = %d, HL = %d avec l'intention %d\n",my_position,HL,1);
      }
    }
    printf("La valeur de HL après la boucle while est %d\n",HL);

    /* On commence par tester l'arrivée d'un message */
    s_service=accept(s_ecoute,(struct sockaddr*) &sock_add_dist,&size_sock);
    if (s_service>0) {
      /*Extraction et affichage du message */
      l=read(s_service,&msg,sizeof(msg));
      texte[l] ='\0';
      printf("Message recu : id : %d hl : %d  intention : %d \n",msg.id,msg.hl,msg.intention); fflush(0);
      // un site nous indique qu'il sort de SC (libération)
      if (msg.intention == 0) { 
        HL = max(HL,msg.hl)+1; // maj de l'horloge 
        tableau_attente[msg.id]=-1; // on réinitialise la valeur de son horloge dans la file d'attente pour entrer en SC 
      }
      // un site veut entrer en SC
      else if (msg.intention == 1){ 
        HL = max(HL,msg.hl)+1; // maj de l'horloge 
        tableau_attente[msg.id]=msg.hl; // on renseigne sa valeur d'horloge au moment de la demande dans la file d'attente
        HL++; //on incrémenta la valeur de HL pour indiquer qu'on envoie la réponse à l'instant d'après
        envoie_msg(my_position,NSites,tableau_socket,tableau_sockaddr,HL,2,msg.id); // envoie d'un message acceptant la requête
        printf("Envoie du message id = %d, HL = %d avec l'intention %d\n",my_position,HL,2);
      }
      // cas où on reçoit un message indiquant qu'un site accepte une demande d'entrée en SC
      else if (msg.intention == 2 ){ 
        // on vérifie si le message est pour nous ou bien un autre site
        if (msg.to==my_position){
          HL = max(HL,msg.hl)+1; // maj de l'horloge 
          tableau_accord[msg.id]=1; // on indique dans notre tableau que le site est d'accord pour que je rentre en SC
        }
        else{
          printf("Le message plus haut ne me concerne pas \n");
        }
      }
      printf("Valeur intermédiare de HL (après quelconque envoie %d)\n",HL);
      
      close (s_service);
    }

    // test pour voir si on peut rentrer en section critique 
    // on regarde si on est le premier de la file d'attente et qu'on a l'accord de tous 
    if (in_SC != 1 && min_tableau(tableau_attente,NSites)==my_position && accord_tous(tableau_accord,NSites)==1){
        HL++;
        printf("Rentré en section critique\n");
        in_SC = 1;
    }

    // partie débogage affiche les tableaux pour voir où l'on se trouve dans l'exécution du code (qui a accepté, qui veut entrer en SC ...)
    printf("tableau attente pos = %d my_position = %d result = %d\n",min_tableau(tableau_attente,NSites),my_position,min_tableau(tableau_attente,NSites)==my_position);
    for (int i = 0; i < NSites; i++){
      printf("%d ",tableau_attente[i]);
    }
    printf("\n");
    printf("tableau accord validation de la fonction (%d)\n",accord_tous(tableau_accord,NSites)==1);
    for (int i = 0; i < NSites; i++){
      printf("%d ",tableau_accord[i]);
    }
    printf("\n");
    printf("test condition (%d)\n",((min_tableau(tableau_attente,NSites)==my_position) && (accord_tous(tableau_accord,NSites)==1)));

    /* Petite boucle d'attente : c'est ici que l'on peut faire des choses*/
    for(l=0;l<10000000;l++) { 
      t=t*3;
      t=t/3;
    }
      
    // cas où on est dans la SC   
    if (in_SC == 1){
      printf("*");fflush(0);
      time_SC++;
      // condition d'arrêt de le SC
      if (time_SC > 5){
        HL++; 
        // envoie d'un message pour indiquer aux autres sites qu'on sort de SC => faut enlever ce site de la file d'attente
        envoie_msg(my_position,NSites,tableau_socket,tableau_sockaddr,HL,0,-1);
        printf("Envoie du message id = %d, HL = %d avec l'intention %d\n",my_position,HL,0);
        tableau_attente[my_position] = -1; // on s'enlève de la liste des sites qui veulent aller en SC
        // on réinitalise la tableau accord car on ne veut plus entrer en SC
        for (int i = 0; i < NSites; i++){
          tableau_accord[i] = 0;
        }
        in_SC = 0;
      }
    }
    else {
      printf(".");fflush(0); /* pour montrer que le serveur est actif*/
    }
    printf("La valeur de HL pour le site %d à la fin de la boucle while est %d\n",my_position,HL);
  }


  close (s_ecoute);  
  return 0;
}