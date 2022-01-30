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
void envoie_msg(int my_position,int NSites, int * tableau_socket, struct sockaddr_in * tableau_sockaddr,int HL, int intention, int to);
int accord_tous(int* tableau_attente,int NSites); // fonction qui renvoie si tous les sites ont donnés leurs accords pour passer dans al SC 
int min_tableau(int* tableau_attente,int NSites); // fonction qui renvoie l'indice de la valeur min du tableau 


// création d'une struct pour tester l'envoie de msg
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

// fonction pour envoyer des messages
void envoie_msg(int my_position,int NSites, int * tableau_socket, struct sockaddr_in * tableau_sockaddr,int HL, int intention, int to){
  Message msg;
  int size_sock = sizeof(struct sockaddr_in);
  for (int i = 0; i <NSites;i++){
    if (i != my_position){
      if ((tableau_socket[i] = socket(AF_INET, SOCK_STREAM,0))==-1){
        perror("Création socket");
        exit(-1);
      }
      if (connect(tableau_socket[i],(struct sockaddr*) &tableau_sockaddr[i],size_sock)==-1){
            perror("connect inter-site");
            exit(-1);
        }
      else{
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

int accord_tous(int* tableau_attente, int NSites){ 
  for (int i = 0; i <NSites;i++){
    if (tableau_attente[i] != 1){
      return -1;
    }
  }
  return 1;
}

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
  my_position = PortBase-main_site; // position du site selon le site de base

  // socket principale ? 
  sock_add.sin_family = AF_INET;
  sock_add.sin_addr.s_addr= htons(INADDR_ANY);  
  sock_add.sin_port = htons(PortBase);

  /* Création des variables pour réaliser l'algo de Lamport */ 
  struct hostent* hp;
  socklen_t size_sock;
  int size_struct;
  int HL = 0; // va stocker la valeur de l'horloge
  int in_SC = 0; // valeur pour savoir si le processus est en SC 
  int temp = 0; 
  int time_SC = 0;
  // on va utiliser la valeur en position tableau_attente[my_position] pour voir si une demande de SC a été faite
  struct sockaddr_in tableau_sockaddr[NSites]; 
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
  // seule le site 0 peut envoyer un msg au début pour le site 1
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
    else if (70<r && r<90 && in_SC != 1){
      HL++; // incrémentation de la valeur de l'horloge'
    }
    else{
      // on envoie une demande pour entrer dans la section critique
      if (tableau_attente[my_position]==-1){ // cas où on n'a pas encore fait de demande pour entrer en SC
        HL++;
        tableau_attente[my_position]=HL;
        tableau_accord[my_position] = 1;
        // envoyer un message pour faire une requête d'entrée en SC
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
      if (msg.intention == 0) { // fin de la SC
        HL = max(HL,msg.hl)+1; // maj de l'horloge 
        tableau_attente[msg.id]=-1;
         
      }
      else if (msg.intention == 1){ // demande pour être en SC
        HL = max(HL,msg.hl)+1; // maj de l'horloge 
        tableau_attente[msg.id]=msg.hl;
        //HL = max(HL,msg.hl)+1; // maj de l'horloge  
        HL++; //on incrémenta la valeur de HL pour indiquer qu'on envoie la réponse à l'instant d'après
        envoie_msg(my_position,NSites,tableau_socket,tableau_sockaddr,HL,2,msg.id);
        printf("Envoie du message id = %d, HL = %d avec l'intention %d\n",my_position,HL,2);
      }
      else if (msg.intention == 2 ){ // cas où on reçoit un accord et que le message est adressé à moi 
        if (msg.to==my_position){
          HL = max(HL,msg.hl)+1; // maj de l'horloge 
          tableau_accord[msg.id]=1;
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
    
  
    //envoie_msg(my_position,NSites,tableau_socket,tableau_sockaddr,HL,0);
      
      
    if (in_SC == 1){
      printf("*");fflush(0);
      time_SC++;
      if (time_SC > 5){
        HL++; 
        // code pour faire la libération
        envoie_msg(my_position,NSites,tableau_socket,tableau_sockaddr,HL,0,-1);
        printf("Envoie du message id = %d, HL = %d avec l'intention %d\n",my_position,HL,0);
        tableau_attente[my_position] = -1; // on s'enlève de la liste des sites qui veulent aller en SC
        // on remet à 0 les accords pour entrer en SC
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