#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <semaphore.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#define taille 50
/*Le client */
int sock;	//descripteur socket
int con;	//descripteur connexion
struct sockaddr_in addr;	// adresse du serveur
pthread_t threadclient;	//thread client
char ipserveur[taille];
int numport;
char nom[taille];	//nom du client

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Sreservation
{
	int idville;	//identifiant de la ville
	char nomclient[taille];	//nom du client
	int mode;	// partagé ou exclusif
	int cpuUtil;
	int nbgoUtil;
	int poubelle;

};	// struct d'une demande client

struct etat
{
	int nbutilisateurs;	//nombre d'utilisateurs
	char geo[taille];	//les villes
	int igeo;	// les villes en int, 0 1 2 3
	int nbcpu;
	int cpupartage;
	int nbgo;
	int nbgopartage;
	struct Sreservation clients[taille];
	int goinitial;
	int cpuinitial;

};	// struct des états

struct etat villes[4];	// tableau contenant les états de toutes les villes

struct etatstruct
{
	struct etat v[4];
	int accusereception;
};	//struct contenant les états pour l'envoie et l'accusé de reception

struct tableaureservation
{
	struct Sreservation tab[20];
	int nbres;
	int villesup;
};
struct etatstruct reponse;	//buffer reponse
struct tableaureservation envoie;	//buffer envoie

pthread_mutex_t v;	// pour vérouiller l'état
pthread_mutex_t v1;

//affichage des clients//////////////////////////////////////////////////////////////////////////////////////////////////////
void afficheclient(struct etat s)
{
	for (int i = 0; i < s.nbutilisateurs; i++)
	{
		if (s.clients[i].poubelle != 99)
		{

			printf("<<<<<<<<<< client : %s >>>>>>>>>>\n", s.clients[i].nomclient);
			if (s.clients[i].mode == 0)
			{
				printf("Mode partagé \n");
			}
			if (s.clients[i].mode == 1)
			{
				printf("Mode exclusif \n");
			}
			printf("Nombre de CPU : %d \n", s.clients[i].cpuUtil);
			printf("Nombre de GO : %d \n", s.clients[i].nbgoUtil);
		}
		else
		{
			printf("------utilisateur supprimé------\n");
		}
	}
}

//affichage des états////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void afficheEtat(struct etat e, char nom[])
{
	printf("******************************************Affichage des états ******************************************\n");
	printf("Ville : %s \n", e.geo);
	printf("Nombre de CPU disponibles : %d \n", e.nbcpu);
	printf("Nombre de GO disponibles : %d \n", e.nbgo);
	if (e.nbutilisateurs > 0)
	{
		afficheclient(e);
	}
	else
	{
		printf("-----Aucun utilisateur-----\n");
	}
	printf("\n");
}

//vérifie si les états contiennent un client avec le nom donné////////////////////////////////////////////////////////////////////////////////////////////
bool contient(struct etat villes[4], char nom[taille])
{
	bool t = false;
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j <= villes[i].nbutilisateurs; j++)
		{
			if (strcmp(villes[i].clients[j].nomclient, nom) == 0)
			{
				t = true;
			}
		}
	}
	return t;
}
//fonction pour saisir les demandes///////////////////////////////////////////////////////////////////////////////////////////////////////////////
void demandereservation()
{

	printf("Combien de réservation voulez vous effectuer(4 au maximum, 1 seule par ville)? \n ");
	scanf("%d", &envoie.nbres);
	if (envoie.nbres < 0)
	{
		printf("Erreur de saisie, fin du procéssus, veuillez relancer. \n");
		close(sock);
		exit(1);
	}
	for (int i = 0; i < envoie.nbres; i++)
	{
		strcpy(envoie.tab[i].nomclient, nom);
		printf("Saisissez la ville de réservation %s. \n Montpellier: 0\n Lyon: 1\n Marseille: 2\n Toulouse: 3\n", nom);
		scanf("%d", &envoie.tab[i].idville);
		printf("Le nombre de CPU à réserver : \n");
		scanf("%d", &envoie.tab[i].cpuUtil);
		printf("Le nombre de Gigas de stockage à réserver : \n");
		scanf("%d", &envoie.tab[i].nbgoUtil);
		printf("Le type de réservation : \n 0 pour le mode partagé\n 1 pour le mode exclusif \n");
		scanf("%d", &envoie.tab[i].mode);
		envoie.villesup = -1;
		if (envoie.nbres < 0 || envoie.nbres > 4)
		{
			printf("Erreur saisie nombre de réservations (4 au maximum, 1 seule par ville).\n");
			demandereservation();
		}
		if ((envoie.tab[i].mode < 0) || (envoie.tab[i].mode > 1))
		{
			printf("Erreur saisie de mode de réservation, réessayer \n");
			demandereservation();
		}
		if ((envoie.tab[i].idville < 0) || (envoie.tab[i].idville > 3))
		{
			printf("Erreur saisie de ville, réessayer. \n");
			demandereservation();
		}
		if ((envoie.tab[i].cpuUtil > villes[envoie.tab[i].idville].cpuinitial) || (envoie.tab[i].nbgoUtil > villes[envoie.tab[i].idville].goinitial))
		{

			printf("Votre demande de ressources est trop excéssives, veuillez réessayer. \n");
			demandereservation();
		}
	}
}

void demander(char nom[taille])
{

	int x;
	int villelibere;

	printf("Que voulez vous faire %s ? \n 0 pour réserver des ressources\n 1 pour supprimer des ressources \n 2 pour se déconnecter\n", nom);
	scanf("%d", &x);

	if (x == 1)
	{

		if (contient(villes, nom) == true)
		{

			printf("Donnez la ville dont les ressources sont à libérer \n Montpellier: 0\n Lyon: 1\n Marseille: 2\n Toulouse: 3\n");

			scanf("%d", &villelibere);

			if ((villelibere > 3) || (villelibere < 0))

			{

				printf("Erreur saisie de ville, réessayer sans faire de fautes svp. \n");

				demander(nom);
			}
			else
			{
				envoie.villesup = villelibere;

				strcpy(envoie.tab[0].nomclient, nom);
			}
		}
		else
		{

			printf("Vous ne possédez aucune ressources à libérer, veuillez réessayer plus tard.\n");

			demander(nom);
		}
	}
	else if (x == 0)
	{

		demandereservation();
	}

	if (x == 2)
	{
		printf("Déconnexion en cours.........\n");
		pthread_exit(NULL);
	}
}
//affichage des états après une modification///////////////////////////////////////////////////////////////////////////////////////////////////////
void *affichage()
{

	while (1)
	{
		int message;
		message = recv(sock, &reponse, sizeof(reponse), 0);

		if (message > 0)
		{

			if (reponse.accusereception == 321)
			{

				system("clear");
				printf("Reception du message du serveur \n");
				printf("Votre demande a été traitée!\n");
			}
			else
			{
				printf("Nouvel mise à jour de l'état!(vous pouvez continuer votre saisi) \n");

				if (pthread_mutex_lock(&v1) != 0)
				{
					printf("erreur verrou v\n");
					pthread_exit(NULL);
				}
				printf("\n");
				for (int i = 0; i < 4; i++)
				{
					villes[i] = reponse.v[i];
				}	//mise à jour des états
				printf("Les états sont mis à jour \n");

				printf("\n");

				for (int i = 0; i < 4; i++)
				{
					afficheEtat(reponse.v[i], nom);
				}

				printf("\n");

				if (pthread_mutex_unlock(&v1) != 0)
				{
					printf("erreur unlock v1 verrou\n");
					pthread_exit(NULL);
				}
			}
		}

		if (message <= 0)
		{
			printf("Erreur socket pendant reception \n");
			printf("Arret procéssus client et fermeture socket...... \n");
			close(sock);
			exit(1);
		}
	}
}
//création d'un client et du thread d'affichage//////////////////////////////////////////////////////////////////////////////////////////
void *initClient()
{

	pthread_t threadaffichage;

	//création verrous

	if (pthread_mutex_init(&v1, NULL) != 0)
	{
		fprintf(stderr, "Erreur création verrou, fermeture socket et arret \n");
		pthread_exit(NULL);
	}

	//thread pour récuperer les étatsS

	if (pthread_create(&threadaffichage, NULL, affichage, NULL) != 0)
	{
		fprintf(stderr, "Erreur création thread d'affichage, arret procéssus \n");
		pthread_exit(NULL);
	}
	///////////////////////////////////////////////////////////////////
	if (pthread_mutex_lock(&v1) != 0)
	{
		printf("erreur verrou\n");
		pthread_exit(NULL);
	}
	printf("Entrez votre nom : \n");
	scanf("%s", nom);

	//affichage des états
	printf("\n");
	for (int i = 0; i < sizeof(villes) / sizeof(villes[0]); i++)
	{
		afficheEtat(villes[i], nom);
	}
	printf("\n");
	printf("\n");

	demander(nom);

	if (pthread_mutex_unlock(&v1) != 0)

	{
		printf("erreur verrou\n");
		pthread_exit(NULL);
	}

	//envoie
	if (send(sock, &envoie, sizeof(envoie), 0) < 0)
	{
		printf("Erreur d'envoie de réservation \n");
		pthread_exit(NULL);
	}

	printf("Demande envoyée pour traitement au serveur! \n");

	//demande qui tourne en boucle
	while (1)
	{

		sleep(1);

		if (pthread_mutex_lock(&v1) != 0)

		{

			printf("erreur verrou\n");
			pthread_exit(NULL);
		}

		demander(nom);

		if (pthread_mutex_unlock(&v1) != 0)
		{
			printf("erreur verrou\n");
			pthread_exit(NULL);
		}

		if (send(sock, &envoie, sizeof(envoie), 0) < 0)
		{
			printf("Erreur d'envoie \n");
			pthread_exit(NULL);
		}

		printf("Demande envoyée pour traitement au serveur, veuillez attendre \n");
		//attente de réponse du serveur(accusereception)

		sleep(2);
	}
}
//main///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main()
{

	//création du socket***************************************************************************
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock != -1)
	{
		printf("Socket client crée.\n");
	}
	else
	{
		printf("Erreur création socket, arret du procéssus\n");
		exit(1);
	}

	printf("Entrez l'adresse IP du serveur. \n");
	fgets(ipserveur, 49, stdin);
	printf("Entrez le numero de port \n");
	scanf("%d", &numport);

	inet_aton(ipserveur, &addr.sin_addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(numport);

	//connéxion au serveur*****************************************************************************
	con = connect(sock, (struct sockaddr *) &addr, sizeof(addr));

	if (con != -1)
	{
		printf("Connexion au serveur effectuée. \n");
	}
	else
	{
		printf("Erreur connexion au serveur, fermeture socket et arret du procéssus\n");
		close(sock);
		exit(1);
	}

	printf("\n");
	//reception des etats************************************************************************************
	if (recv(sock, &reponse, sizeof(reponse), 0) > 0)
	{
		printf("Les états ont été correctement recus du serveur. \n");
	}
	else
	{
		printf("Erreur de reception des états ou fermeture socket du serveur\n ");
		printf("Arret du procéssus \n");
		close(sock);
		exit(1);
	}
	for (int i = 0; i < 4; i++)
	{
		villes[i] = reponse.v[i];
	}

	//création du thread client **********************************************************************************

	if (pthread_create(&threadclient, NULL, initClient, NULL) != 0)
	{
		fprintf(stderr, "Erreur création thread client!!! \n");
		close(sock);
		exit(1);
	}
	//attente de la fin du thread***********************************************************************************
	if (pthread_join(threadclient, NULL) != 0)
	{
		fprintf(stderr, "pthread_join error!!! \n");
		close(sock);
		exit(1);
	}
	//fermeture du socket*******************************************************************************************
	if (close(sock) != 0)
	{
		printf("Erreur fermeture socket, arret du procéssus \n");
		exit(1);
	}
	else
	{
		printf("Socket fermée avec succès, fin du procéssus ! \n");
	}

	return 1;
}