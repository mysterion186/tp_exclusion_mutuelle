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

// création d'une struct pour tester l'envoie de msg
struct Message {
  int a; 
  int b;
};
typedef struct Message Message;


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
  int size_sock;
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

/***********************************************************************/
/***********************************************************************/
/***********************************************************************/
/***********************************************************************/

int main (int argc, char* argv[]) {
  struct sockaddr_in sock_add, sock_add_dist,sock_envoie;
  struct hostent* hp;
  int size_sock;
  int size_struct,envoie;
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

  /*
    Communication entre site
  */
 struct sockaddr_in tableau[NSites]; 
 // Recup adresse tous les sites sont en localhost donc pas de soucis
  hp = gethostbyname("localhost");
  if (hp == NULL) {
		perror("client");
		return -1;
  }
  size_struct = sizeof(struct sockaddr_in);
 // seule le site 0 peut envoyer un msg au début pour le site 1
 if (my_position==0){
   tableau[1].sin_family = AF_INET;
   tableau[1].sin_port = htons(main_site+1);
   memcpy(&tableau[1].sin_addr.s_addr, hp->h_addr, hp->h_length);
   tableau[2].sin_family = AF_INET;
   tableau[2].sin_port = htons(main_site+2);
   memcpy(&tableau[2].sin_addr.s_addr, hp->h_addr, hp->h_length);
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
  printf("Avant le getsiepos");
  
  printf("\n\n%d\n\n\n",my_position);
  
  /* Passage en mode non bloquant du accept pour tous*/
  /*---------------------------------------*/
  fcntl(s_ecoute,F_SETFL,O_NONBLOCK);
  size_sock=sizeof(struct sockaddr_in);
  
  /* Boucle infini*/
  while(1) {
  
    srand(time(NULL));
    int r = rand();

    if (r <= 70){
      // on ne fait rien 
    }
    else if (70<r && r<90){
      // on fait incrémenter la valeur de HL
    }
    else{
      // on envoie une demande pour entrer dans la section critique
    }
    /* On commence par tester l'arrivée d'un message */
    s_service=accept(s_ecoute,(struct sockaddr*) &sock_add_dist,&size_sock);
    if (s_service>0) {
      /*Extraction et affichage du message */
      l=read(s_service,&msg,sizeof(msg));
      texte[l] ='\0';
      printf("Message recu : %d/%d \n",msg.a,msg.b); fflush(0);
      close (s_service);
    }


    /* Petite boucle d'attente : c'est ici que l'on peut faire des choses*/
    for(l=0;l<10000000;l++) { 
      t=t*3;
      t=t/3;
    }
    if (my_position==0){
      if ( (envoie=socket(AF_INET, SOCK_STREAM,0))==-1) {
      perror("Creation socket");
      exit(-1);
    }
      if (connect(envoie,(struct sockaddr*) &tableau[1],size_sock)){
        perror("connect inter-site");
      }
      else{
        msg.a = 3;
        msg.b = 10;
        write(envoie,&msg,sizeof(msg));
        close(envoie);
      }
      
      if ( (envoie=socket(AF_INET, SOCK_STREAM,0))==-1) {
      perror("Creation socket");
      exit(-1);
    }
      if (connect(envoie,(struct sockaddr*) &tableau[2],size_sock)){
        perror("connect inter-site");
      }
      else{
        msg.a = 3;
        msg.b = 10;
        write(envoie,&msg,sizeof(msg));
        close(envoie);
      }
      
    }
    printf(".");fflush(0); /* pour montrer que le serveur est actif*/
  }


  close (s_ecoute);  
  return 0;
}

