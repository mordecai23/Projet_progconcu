Pour compiler le serveur : gcc -pthread -o serveur serveur.c

Pour compiler le client : gcc -pthread -o client client.c

Comment lancer le programme?

Tout d'abord compiler le serveur.c (dans le dossier serveur) avec la commande précédente afin de produire l'exécutable "serveur".
Ensuite compiler le fichier client.c(dans le dossier client) afin de produire l'exécutable "client".
Exécutez le "serveur", et donnez le numéro de port.
Ensuite exécutez le "client" et donnez l'adresse ip du serveur et le numéro de port.
Vous pouvez lancer le "client" sur plusieurs terminales afin de simuler le fait d'avoir plusieurs clients connectés au serveur.

Comment utiliser le programme?

Dans le terminal du client, sélectionnez ce que vous voulez faire (réservation ,suppression, déconnexion).

1) Réservation : 
vous pouvez faire 4 réservations au maximum (dans 4 villes différentes ou les mêmes , sauf si les réservations dans la même ville dépassent la limite de ressources disponibles)
Par exemple, ne pas réserver 100 go à toulouse ensuite 50 go à toulouse dans la même saisie, tout en exclusif, si il y que 100 go à Toulouse.
Une fois saisie, les réservations sont envoyées au serveur, et le client attend la réponse (un message informant le client que sa réservation est effectuée).
En attente de la réponse du serveur, vous pouvez saisir d'autre demandes (qui seront envoyées au serveur, mais pas traitées avant que la réservation précédente soit effectuée)
Vous recevrez l'état des ressources à chaque mise à jour de l'état (réservation ou suppression).

2) Suppression
Vous pouvez supprimer vos réservations dans une ville, en donnant le numéro de la ville.
Elle vérifiera d'abord si vous avez des réservation dans l'état avant de l'envoyer au serveur. 
Cette requête supprimera toutes vos réservations (exclusives ou partagées) dans la ville donnée.

3) Déconnexion
Cette option vous déconnecte du serveur,ferme la socket client du serveur, tue le processus fils correspondant du serveur, ferme la socket coté client et enfin termine le processus client.
