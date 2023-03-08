#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>

/* codul de eroare returnat de anumite apeluri */
extern int errno;
/* portul de conectare la server*/
int port;

int main (int argc, char *argv[])
{
  int optval=1; 		/* optiune folosita pentru setsockopt()*/ 
  int nfds;			/* numarul maxim de descriptori */
  struct timeval tv;		/* structura de timp pentru select() */
  fd_set readfds;		/* multimea descriptorilor de citire */
  fd_set actfds;		/* multimea descriptorilor activi */
  int sd;			// descriptorul de socket
  struct sockaddr_in server;	// structura folosita pentru conectare 
  char msg[1000];		// mesajul trimis
  int temp;
  /* exista toate argumentele in linia de comanda? */
  if (argc != 3)
    {
      printf ("Sintaxa: %s [adresa_server] [port]\n", argv[0]);
      return -1;
    }
  /* stabilim portul */
  port = atoi (argv[2]);
  /* cream socketul */
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
  {
      perror ("Eroare la socket().\n");
      return errno;
  }
  /* umplem structura folosita pentru realizarea conexiunii cu serverul */
  /* familia socket-ului */
  server.sin_family = AF_INET;
  /* adresa IP a serverului */
  server.sin_addr.s_addr = inet_addr(argv[1]);
  /* portul de conectare */
  server.sin_port = htons (port);
  /* ne conectam la server */
  if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
  {
      perror ("[client]Eroare la connect().\n");
      return errno;
  }
  FD_ZERO (&actfds);		/* initial, multimea este vida */
  FD_SET (sd, &actfds);		/* includem in multime socketul creat */
  FD_SET (0, &actfds);		/* includem in multime socketul creat */
  tv.tv_sec = 1;		/* se va astepta un timp de 1 sec. */
  tv.tv_usec = 0;
  /* valoarea maxima a descriptorilor folositi */
  nfds = sd;
  while(1)
  {
      bzero (msg, 1000);
      bcopy ((char *) &actfds, (char *) &readfds, sizeof (readfds));
      /* apelul select() */
      if (select (nfds+1, &readfds, NULL, NULL, &tv) < 0)
      {
          perror ("[server] Eroare la select().\n");
          return errno;
      }
       /* citirea mesajului, daca exista */
      if (FD_ISSET (sd, &readfds))
      {
          /* citirea raspunsului dat de server*/
          if ((temp=read (sd, msg, 1000)) < 0)
          {  
              perror ("[client]Eroare la read() de la server.\n");
              return errno;
          }
          /* afisam mesajul primit */
          if(strncmp(msg,"quit",4)==0) //exit
          {
              /* inchidem conexiunea, am terminat */
              printf ("[client]%d Mesajul primit este: %s\n", temp, msg);
              close (sd);
              break;
          }
          printf ("[client]%d Mesajul primit este: %s\n [client] Puteti introduce o comanda: ", temp, msg);
          fflush(stdout);
      }
      if(FD_ISSET (0, &readfds))
      {
          if ((temp=read (0, msg, 1000)) < 0)
          {  
          perror ("[client]Eroare la read() de la server.\n");
          return errno;
          }
          /* trimiterea mesajului la server */
          if (write (sd, msg, 1000) <= 0)
          {
              perror ("[client]Eroare la write() spre server.\n");
              return errno;
          }
          printf ("[client]%d Mesajul trimis cu succes\n", temp);
      }
  }
}