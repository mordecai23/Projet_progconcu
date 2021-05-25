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
#define c1 "sem1.txt"
#define c2 "sem2.txt"
#define c3 "sem3.txt"
#define c4 "memoire.txt"
#define taille_shm 1024
#define taille 50
////////////////////////////////////////////////////Les unions des sémaphores///////////////////////////////////////////////////////////////
union semun u1;
//semaphore 1 pour protéger les états 

union semun u2;
//semaphore 2 pour la notification

union semun u3;
//sem3 pour notifier les supression aux procéssus en attente

struct sembuf p = { 0, -1, SEM_UNDO
};	// semwait
struct sembuf v = { 0, +1, SEM_UNDO
};	// semsignal
struct sembuf z = { 0, 0, SEM_UNDO
};

//////////////////////////////////////socket du fils/////////////////////////////////////////////////
int socketfils;
int numport;

//Les structs	///////////////////////////////////////////////////////////////////////////////////////////////////////////
struct Sreservation
{
	int idville;
	char nomclient[taille];
	int mode;
	int cpuUtil;
	int nbgoUtil;
	int poubelle;	//savoir si un utilisateur a été supprimé

};	// struct d'une demande client

struct etat
{
	int nbutilisateurs;	//nombre d'utilisateurs
	char geo[taille];	//les villes
	int igeo;	// les villes en int, 0 1 2 3
	int nbcpu;	//cpu disponible
	int cpupartage;
	int nbgo;	//go disponible
	int nbgopartage;
	struct Sreservation clients[taille];	//contient les clients
	int goinitial;
	int cpuinitial;

};	// struct des C)tats

struct etat villes[4];	// tableau contenant les états de toutes les villes

struct tableaureservation
{
	struct Sreservation tab[20];
	int nbres;
	int villesup;
};

struct etatstruct
{

	struct etat v[4];
	int accusereception;
};

//struct contenant les C)tats pour l'envoie

struct etatstruct reception;	//contient l'accusé de reception

struct etatstruct * etatshm;	//mémoire partagée

struct etatstruct etat;

struct tableaureservation reponse;	//contient la demande de réservation

union semun
{
	int val;
	struct semid_ds * buf;
	unsigned short * array;
};

////////////////////les id des sémaphores/////////////////////////////////////////////
int id1;
int id2;
int id3;

//les fonctions///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
			printf("-----utilisateur supprimé-----\n");
		}
	}
}
//affichage des étatq
void afficheEtat(struct etat e)
{
	printf("******************************************Affichage des états ******************************************\n");
	printf("ville : %s \n", e.geo);
	printf("nombre de CPU disponibles : %d \n", e.nbcpu);
	printf("nombre de GO disponibles : %d \n", e.nbgo);
	if (e.nbutilisateurs > 0)
	{
		afficheclient(e);
	}
	else
	{
		printf("-----Aucun utilisateur-----\n");
	}
}

//si la réservation peut etre acceptée, renvoit 0 si possible, -1 sinon
int traitementpossible(struct Sreservation res, int v)
{
	int x = -1;
	if (res.mode == 0 && (res.nbgoUtil <= villes[v].nbgo || res.nbgoUtil <= villes[v].nbgo - villes[v].nbgopartage) && (res.cpuUtil <= villes[v].nbcpu || res.cpuUtil <= villes[v].nbcpu - villes[v].cpupartage))
	{
		x = 0;
	}
	if (res.mode == 1 && res.nbgoUtil <= villes[v].nbgo - villes[v].nbgopartage && res.cpuUtil <= villes[v].nbcpu - villes[v].cpupartage)
	{
		x = 0;
	}

	return x;

}
//vérifie si les réservations sont possibles pour un ensemble de reservation
int traitementreservation(struct tableaureservation res)
{

	int x = 0;
	for (int i = 0; i < res.nbres; i++)
	{
		x = x + traitementpossible(res.tab[i], res.tab[i].idville);
	}
	return x;

}
//fonction pour traiter les demandes acceptées
void traitementdemande(struct Sreservation res, int v)
{
	if (res.mode == 1)
	{
		villes[v].nbcpu = villes[v].nbcpu - res.cpuUtil;
		villes[v].nbgo = villes[v].nbgo - res.nbgoUtil;
		villes[v].clients[villes[v].nbutilisateurs] = res;
		villes[v].nbutilisateurs = villes[v].nbutilisateurs + 1;
		printf("Demande acceptée : %s \n", res.nomclient);
	}
	if (res.mode == 0)
	{
		villes[v].clients[villes[v].nbutilisateurs] = res;

		villes[v].nbutilisateurs = villes[v].nbutilisateurs + 1;

		if (res.nbgoUtil > villes[v].nbgopartage)
		{
			villes[v].nbgopartage = villes[v].nbgopartage + (res.nbgoUtil - villes[v].nbgopartage);
		}

		if (res.cpuUtil > villes[v].cpupartage)
		{
			villes[v].cpupartage = villes[v].cpupartage + (res.cpuUtil - villes[v].cpupartage);
		}

		printf("Demande acceptée : %s \n", res.nomclient);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//calcul le maximum cpu utilisé en partagé par un utilisateur autre que "nom"
int maxcpu(struct Sreservation tab[taille], char nom[taille], int nb)
{
	int max = 0;
	for (int i = 0; i <= nb; i++)
	{
		if (tab[i].mode == 0 && strcmp(tab[i].nomclient, nom) != 0)
		{
			if (tab[i].cpuUtil > max)
			{
				max = tab[i].cpuUtil;
			}
		}
	}
	return max;
}

int maxgo(struct Sreservation tab[taille], char nom[taille], int nb)
{
	int max = 0;
	for (int i = 0; i <= nb; i++)
	{
		if (tab[i].mode == 0 && strcmp(tab[i].nomclient, nom) != 0)
		{
			if (tab[i].nbgoUtil > max)
			{
				max = tab[i].nbgoUtil;
			}
		}
	}
	return max;
}
//supprime un utilisateur de l'état d'une ville
void supprimer(int ville, char nom[taille])
{
	int y = villes[ville].nbutilisateurs;

	for (int i = 0; i < y; i++)
	{
		if (strcmp(villes[ville].clients[i].nomclient, nom) == 0)
		{
			if (villes[ville].clients[i].mode == 1)
			{
				villes[ville].nbcpu = villes[ville].nbcpu + villes[ville].clients[i].cpuUtil;

				villes[ville].nbgo = villes[ville].nbgo + villes[ville].clients[i].nbgoUtil;

				villes[ville].clients[i].poubelle = 99;
			}

			if (villes[ville].clients[i].mode == 0)
			{
				villes[ville].cpupartage = maxcpu(villes[ville].clients, nom, villes[ville].nbutilisateurs);

				villes[ville].nbgopartage = maxgo(villes[ville].clients, nom, villes[ville].nbutilisateurs);

				villes[ville].clients[i].poubelle = 99;
			}
		}
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//fonction pour envoyer les états au client à chaque mis à jour
void *affichage()
{

	while (1)
	{
		if (semop(id2, &z, 1) == -1) //on attend que id2==0
		{
			printf("Erreur p semop semaphore");
			printf("Arret procéssus fils \n");
			close(socketfils);
			kill(getpid(), SIGINT);
		}
		if (semop(id1, &p, 1) == -1)
		{
			printf("Erreur p semop semaphore");
			printf("Arret procéssus fils \n");
			close(socketfils);
			kill(getpid(), SIGINT);
		}
		etat = *etatshm;

		if (send(socketfils, &etat, sizeof(etat), 0) < 0)
		{
			printf("Erreur d'envoie état au client \n");
			printf("Arret procéssus fils \n");
			close(socketfils);
			kill(getpid(), SIGINT);
		}
		printf("Mise à jour envoyé aux clients \n");
		if (semop(id1, &v, 1) == -1)
		{
			printf("Erreur p semop semaphore");
			printf("Arret procéssus fils \n");
			close(socketfils);
			kill(getpid(), SIGINT);
		}
		int semval2 = semctl(id2, 0, GETVAL, u2);

		if (semval2 == 0) //vérifie si la valeur de id2==0

		{

			if (semop(id2, &v, 1) == -1)
			{
				printf("Erreur p semop semaphore");
				printf("Arret procéssus fils \n");
				close(socketfils);
				kill(getpid(), SIGINT);
			}
		}
	}
}

int
main()
{
	////////////////////////////////création des clés/////////////////////////////////////////////////

	key_t cle1 = ftok(c1, 'd');
	key_t cle2 = ftok(c2, 'c');
	key_t cle3 = ftok(c3, 'b');
	key_t cle4 = ftok(c4, 'a');
//////////////////////valeur des sémaphores///////////////////////////////////////////
	u1.val = 1;
	u2.val = 1;
	u3.val = 1;
	//les semaphores////////////////////////////////////////////////////////
	id1 = semget(cle1, 1, 0666 | IPC_CREAT);
	id2 = semget(cle2, 1, 0666 | IPC_CREAT);
	id3 = semget(cle3, 1, 0666 | IPC_CREAT);

	if (id1 == -1)
	{
		printf("Erreur semget semaphore \n");
		exit(1);
	}
	if (id2 == -1)
	{
		printf("Erreur semget semaphore \n");
		exit(1);
	}
	if (id3 == -1)
	{
		printf("Erreur semget semaphore \n");
		exit(1);
	}

	if (semctl(id1, 0, SETVAL, u1) == -1)
	{
		printf("Erreur semctl semaphore \n");
		exit(1);
	}

	if (semctl(id2, 0, SETVAL, u2) == -1)
	{
		printf("Erreur semctl semaphore \n");
		exit(1);
	}

	if (semctl(id3, 0, SETVAL, u3) == -1)
	{
		printf("Erreur semctl semaphore \n");
		exit(1);
	}
	printf("Les sémaphores crées avec succès \n");

	//////////////////////////////////////////////////les états///////////////////////////////////////////////////////////////////////////////
	struct etat e1;
	struct etat e2;
	struct etat e3;
	struct etat e4;

	strcpy(e1.geo, "Montpellier");
	e1.nbcpu = 60;
	e1.nbgo = 500;
	e1.nbutilisateurs = 0;
	e1.igeo = 0;
	e1.cpupartage = 0;
	e1.nbgopartage = 0;
	e1.goinitial = 500;
	e1.cpuinitial = 60;

	strcpy(e2.geo, "Lyon");
	e2.nbcpu = 45;
	e2.nbgo = 630;
	e2.nbutilisateurs = 0;
	e2.igeo = 1;
	e2.cpupartage = 0;
	e2.nbgopartage = 0;
	e2.goinitial = 630;
	e2.cpuinitial = 45;

	strcpy(e3.geo, "Marseille");
	e3.nbcpu = 140;
	e3.nbgo = 1000;
	e3.nbutilisateurs = 0;
	e3.igeo = 2;
	e3.cpupartage = 0;
	e3.nbgopartage = 0;
	e3.goinitial = 1000;
	e3.cpuinitial = 140;

	strcpy(e4.geo, "Toulouse");
	e4.nbcpu = 130;
	e4.nbgo = 1200;
	e4.nbutilisateurs = 0;
	e4.igeo = 3;
	e4.cpupartage = 0;
	e4.nbgopartage = 0;
	e4.goinitial = 1200;
	e4.cpuinitial = 130;

	printf("Les états sont initialisés.\n");

	/////////////////////////////////////////////////////////////SHM///////////////////////////////////////////////////////////////////////

	int shmid;

	shmid = shmget(cle4, sizeof(struct etatstruct), 0666 | IPC_CREAT);

	if (shmid == -1)
	{
		printf("Erreur shmget \n");

		shmctl(id1, IPC_RMID, NULL);
		shmctl(id2, IPC_RMID, NULL);
		shmctl(id3, IPC_RMID, NULL);

		exit(1);
	}

	etat.v[0] = e1;
	etat.v[1] = e2;
	etat.v[2] = e3;
	etat.v[3] = e4;

	etatshm = (struct etatstruct *) shmat(shmid, NULL, 0);

	if ((void*) etatshm == (void*) - 1)
	{
		printf("Erreur shmat \n");
		shmctl(shmid, IPC_RMID, NULL);
		shmctl(id1, IPC_RMID, NULL);
		shmctl(id2, IPC_RMID, NULL);
		shmctl(id3, IPC_RMID, NULL);
		exit(1);
	}

	*etatshm = etat; //on met les états dans la shm

	printf("Les états mis en mémoire partagée.\n");
	printf("\n");

	afficheEtat(etatshm->v[0]);
	afficheEtat(etatshm->v[1]);
	afficheEtat(etatshm->v[2]);
	afficheEtat(etatshm->v[3]);

	printf("\n \n");

	/////////////////////////////////////Sockets////////////////////////////////////////////////////////////////////////////////////
	struct sockaddr_in serveur;

	struct sockaddr_in adresseclient[200];

	//création socket
	int socketS;

	socketS = socket(AF_INET, SOCK_STREAM, 0);
	if (socketS == -1)
	{
		printf(" erreur lors de la création de la socket %s\n", " ");

		shmdt(etatshm);
		shmctl(shmid, IPC_RMID, NULL);
		shmctl(id1, IPC_RMID, NULL);
		shmctl(id2, IPC_RMID, NULL);
		shmctl(id3, IPC_RMID, NULL);
		exit(1);
	}
	else
	{
		printf("Socket serveur (idsocket=%d) crée. \n", socketS);
	}

	// Assignation de l'addresse IP et du port 

	printf("Entrez le numéro de Port du serveur. \n");
	scanf("%d", &numport);

	serveur.sin_family = AF_INET;
	serveur.sin_addr.s_addr = htonl(INADDR_ANY);
	serveur.sin_port = htons(numport);

	//printf("% \n",)

	// liaison de la socket  C  une ip 

	if ((bind(socketS, (struct sockaddr *) &serveur, sizeof(serveur)) != 0))
	{
		printf("Erreur bind du socket serveur\n");
		close(socketS);

		shmdt(etatshm);
		shmctl(shmid, IPC_RMID, NULL);
		shmctl(id1, IPC_RMID, NULL);
		shmctl(id2, IPC_RMID, NULL);
		shmctl(id3, IPC_RMID, NULL);
		exit(1);
	}
	else
	{
		printf("Binding du socket effectué. \n");
	}

	// Attente des client : Serveur en ecoute

	if (listen(socketS, 200) != 0)
	{

		printf(" Erreur listen socket %s\n", " ");
		close(socketS);

		shmdt(etatshm);
		shmctl(shmid, IPC_RMID, NULL);
		shmctl(id1, IPC_RMID, NULL);
		shmctl(id2, IPC_RMID, NULL);
		shmctl(id3, IPC_RMID, NULL);
		exit(1);
	}
	else
	{
		printf("Socket en écoute..... \n");
	}

	//acceptation des clients
	int nbclient = 0;
	printf("Attente connexion des clients............ \n");
	while (1)
	{
		socklen_t length = sizeof(adresseclient[nbclient]);

		int connection = accept(socketS, (struct sockaddr *) &adresseclient[nbclient], &length);

		if (connection < 0)
		{
			printf("Erreur acceptation client \n");
			close(socketS);

			shmdt(etatshm);
			shmctl(shmid, IPC_RMID, NULL);
			shmctl(id1, IPC_RMID, NULL);
			shmctl(id2, IPC_RMID, NULL);
			shmctl(id3, IPC_RMID, NULL);
			exit(0);
		}
		if (connection > 0)

		{

			nbclient++;
			printf("Un nouveau client (socket client= %d) s'est connecté au serveur \n", connection);
			int pid;
			pid = fork();

			if (pid == -1)
			{
				printf
					("Le fork() a échoué \n");
			}
			else if (pid == 0)
			{
			 	//procéssus fils
				//Envoie de l'état au client
				printf("Procéssus fils du client %d crée. \n", connection);
				socketfils = connection;
				pthread_t threadaffichage;

				if (semop(id1, &p, 1) == -1)
				{
					printf("Erreur p semop semaphore");
					printf("Arret procéssus fils \n");
					close(socketfils);
					kill(getpid(), SIGKILL);
				}

				if (send(socketfils, etatshm, sizeof(*etatshm), 0) <= 0)
				{
					printf("Erreur d'envoie de l'état au nouveau client, le procéssus fils s'arrete \n");
					close(socketfils);
					kill(pid, SIGKILL);
				}
				if (semop(id1, &v, 1) == -1)
				{
					printf("Erreur p semop semaphore");
					printf("Arret procéssus fils \n");
					close(socketfils);
					kill(getpid(), SIGKILL);
				}

				//création du thread envoie;
				if (pthread_create(&threadaffichage, NULL, affichage, NULL) != 0)
				{
					fprintf(stderr, "pthread_create error!!! \n");
					printf("Arret procéssus fils \n");
					close(socketfils);
					kill(getpid(), SIGKILL);
				}

				//boucle de reception des demandes
				while (1)
				{

					int demandeclient;
					demandeclient = recv(socketfils, &reponse, sizeof(reponse), 0);

					if (demandeclient == 0)
					{
						printf("Un client s'est déconnecté : fin de ce  procéssus fils et fermeture de la socket %d.\n", connection);
						printf("Arret procéssus fils \n");
						close(socketfils);
						kill(getpid(), SIGKILL);
					}

					if (demandeclient > 0)
					{
						if (reponse.villesup != -1)	//si c'est une suppression///////////////////////////////////////////////////
						{
							if (semop(id1, &p, 1) == -1)
							{
								printf("Erreur p semop semaphore");
								printf("Arret procéssus fils \n");
								close(socketfils);
								kill(getpid(), SIGINT);
							}
							printf("Demande de suppression de %s .\n", reponse.tab[0].nomclient);
							villes[0] = etatshm->v[0];
							villes[1] = etatshm->v[1];
							villes[2] = etatshm->v[2];
							villes[3] = etatshm->v[3];

							supprimer(reponse.villesup, reponse.tab[0].nomclient);	//supression des reservation dans le tableau ville

							//mis à jour de la shm
							etatshm->v[0] = villes[0];
							etatshm->v[1] = villes[1];
							etatshm->v[2] = villes[2];
							etatshm->v[3] = villes[3];

							printf("Supression de %s effectuée. \n", reponse.tab[0].nomclient);
							printf("\n \n");
							afficheEtat(etatshm->v[0]);
							afficheEtat(etatshm->v[1]);
							afficheEtat(etatshm->v[2]);
							afficheEtat(etatshm->v[3]);
							printf("\n \n");

							if (semop(id1, &v, 1) == -1)
							{
								printf("Erreur p semop semaphore");
								printf("Arret procéssus fils \n");
								close(socketfils);
								kill(getpid(), SIGINT);
							}

							if (semop(id2, &p, 1) == -1) //envoie signal pour mis à jour client
							{
								printf("Erreur p semop semaphore");
								printf("Arret procéssus fils \n");
								close(socketfils);
								kill(getpid(), SIGINT);
							}
							if (semop(id3, &p, 1) == -1)//envoie signal pour potentiellement débloquer les clients en attente
							{
								printf("Erreur p semop semaphore");
								printf("Arret procéssus fils \n");
								close(socketfils);
								kill(getpid(), SIGINT);
							}
						}

						//Réservation de ressources
						else if (reponse.villesup == -1)
						{
							if (semop(id1, &p, 1) == -1)
							{
								printf("Erreur p semop semaphore");
								printf("Arret procéssus fils \n");
								close(socketfils);
								kill(getpid(), SIGINT);
							}
							printf("Traitement réservation de %s en cours......\n", reponse.tab[0].nomclient);
							villes[0] = etatshm->v[0];
							villes[1] = etatshm->v[1];
							villes[2] = etatshm->v[2];
							villes[3] = etatshm->v[3];

							while (traitementreservation(reponse) != 0) //tant que traitement pas possible
							{
								if (semop(id1, &v, 1) == -1)
								{
									printf("Erreur v semop semaphore");
									printf("Arret procéssus fils \n");
									close(socketfils);
									kill(getpid(), SIGINT);
								}
								if (semop(id3, &z, 1) == -1) //on attend le signal de suppression
								{
									printf("Erreur z semop semaphore");
									printf("Arret procéssus fils \n");
									close(socketfils);
									kill(getpid(), SIGINT);
								}
								if (semop(id1, &p, 1) == -1)
								{
									printf("Erreur p semop semaphore \n");
									printf("Arret procéssus fils \n");
									close(socketfils);
									kill(getpid(), SIGINT);
								}

								villes[0] = etatshm->v[0];
								villes[1] = etatshm->v[1];
								villes[2] = etatshm->v[2];
								villes[3] = etatshm->v[3];

								int semval = semctl(id3, 0, GETVAL, u3);

								if (semval == 0)
								{
									if (semop(id3, &v, 1) == -1)
									{
										printf("Erreur v semop semaphore\n");
										printf("Arret procéssus fils \n");
										close(socketfils);
										kill(getpid(), SIGINT);
									}
								}
							}
							for (int i = 0; i < reponse.nbres; i++)
							{

								traitementdemande(reponse.tab[i], reponse.tab[i].idville); //on traite chaque demande
							}
							//mettre à jour la shm
							etatshm->v[0] = villes[0];
							etatshm->v[1] = villes[1];
							etatshm->v[2] = villes[2];
							etatshm->v[3] = villes[3];

							printf("Réservation de %s effectuée, affichage du nouvel état. \n", reponse.tab[0].nomclient);
							printf("\n \n ");
							afficheEtat(etatshm->v[0]);
							afficheEtat(etatshm->v[1]);
							afficheEtat(etatshm->v[2]);
							afficheEtat(etatshm->v[3]);
							printf("\n \n");

							if (semop(id1, &v, 1) == -1)
							{
								printf("Erreur v semop semaphore\n");
								printf("Arret procéssus fils \n");
								close(socketfils);
								kill(getpid(), SIGINT);
							}
							if (semop(id2, &p, 1) == -1)
							{
								printf("Erreur p semop semaphore\n");
								printf("Arret procéssus fils \n");
								close(socketfils);
								kill(getpid(), SIGINT);
							}
						}

						reception.accusereception = 321;

						if (send(socketfils, &reception, sizeof(reception), 0) < 0) //envoie accusé de reception au client pour l'informer que sa demande est validée
						{
							printf("Erreur d'envoie accusé reception \n");
							printf("Arret procéssus fils \n");
							close(socketfils);
							kill(getpid(), SIGKILL);
						}
						else
						{
							printf("Accusé de reception envoyé à %s \n", reponse.tab[0].nomclient);
						}
					}
				}
			}
		}
	}
	//fermeture socket et destruction des ipcs
    close(socketS);
	shmdt(etatshm);
	shmctl(shmid, IPC_RMID, NULL);
	shmctl(id1, IPC_RMID, NULL);
	shmctl(id2, IPC_RMID, NULL);
	shmctl(id3, IPC_RMID, NULL);
	return 1;
}