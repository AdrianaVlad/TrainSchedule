#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <arpa/inet.h>
#include <string.h>
#include <libxml2/libxml/parser.h>
#include <libxml2/libxml/tree.h>
#include <libxml2/libxml/xpath.h>
#include <libxml2/libxml/xpathInternals.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#define PORT 2728 /* portul folosit */

void Trimite(int fd, char *msgrasp, int bytes);
int GetTime();
int ReincarcaLista(int fd, const xmlChar *command, char* nr_min);
static void *Refresh(void * arg);
static void *treat(void * arg);
int ProcesareCmd(int fd);
int status_xpath(xmlNodeSetPtr nodes, int opt);
int intarziere_xpath(xmlNodeSetPtr nodes, char * nr_min);
int print_xpath(xmlNodeSetPtr nodes, int fd);
int exec_xpath(const char* filename, const xmlChar* xpathExpr, int fd, char *  nr_min);

extern int errno;		/* eroarea returnata de unele apeluri */
typedef struct thData{
	int idThread; //id-ul thread-ului tinut in evidenta de acest program
	int cl; //descriptorul intors de accept
}thData;
/* functie de convertire a adresei IP a clientului in sir de caractere */
char * conv_addr (struct sockaddr_in address)
{
  static char str[25];
  char port[7];

  /* adresa IP a clientului */
  strcpy (str, inet_ntoa (address.sin_addr));	
  /* portul utilizat de client */
  bzero (port, 7);
  sprintf (port, ":%d", ntohs (address.sin_port));	
  strcat (str, port);
  return (str);
}
//globale deoarece folosim si in main si in alte functii
int sd;		/* descriptori de socket */
char *bd;
int clienti[100],nrc;  //lista fd clienti, si nr de clienti
/* programul */
char cmd_azi[]="//* [number(./zi)=0]/* | //* [number(./zi)=0] | //* [number(./zi)=0]/ancestor::* | //*  [number(./zi)=0]/ancestor::tren/cod";

int main (int argc, char *argv[])
{
  struct sockaddr_in server;	/* structurile pentru server si clienti */
  struct sockaddr_in from;
  int optval=1; 			/* optiune folosita pentru setsockopt()*/ 
  pthread_t th;    //Identificator thread; nu folosim joins / trimitem semnale, deci nu trebuie retinute ID urile
  int i=0;				   
  int client;
  int len;			/* lungimea structurii sockaddr_in */
  if (argc < 2) 
  {
      printf("Sintaxa: %s [numefisier.xml]\n", argv[0]);
      return -1;
  }
  bd = argv[1];
  //initializam baza de date a trenurilor de azi
  /* creare socket */
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
  {
      perror ("[server] Eroare la socket().\n");
      return errno;
  }
  /*setam pentru socket optiunea SO_REUSEADDR */ 
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,&optval,sizeof(optval));
  /* pregatim structurile de date */
  bzero (&server, sizeof (server));
  /* umplem structura folosita de server */
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl (INADDR_ANY);
  server.sin_port = htons (PORT);
  /* atasam socketul */
  if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
  {
      perror ("[server] Eroare la bind().\n");
      return errno;
  }
  /* punem serverul sa asculte daca vin clienti sa se conecteze */
  if (listen (sd, 5) == -1)
  {
      perror ("[server] Eroare la listen().\n");
      return errno;
  }
  /* thread de update a statusului */
  thData * td;
  td=(struct thData*)malloc(sizeof(struct thData));
  td->idThread=i++;
  td->cl=0;
  pthread_create(&th, NULL, &Refresh, td);
  /* servim in mod concurent clientii... */
  while (1)
  {
      int client;
      thData * td; //parametru functia executata de thread     
      int length = sizeof (from);
      printf ("[server]Asteptam la portul %d...\n",PORT);
      fflush (stdout);
      /* acceptam un client (stare blocanta pina la realizarea conexiunii) */
      if ( (client = accept (sd, (struct sockaddr *) &from, &length)) < 0)
      {
	  perror ("[server]Eroare la accept().\n");
	  continue;
      }
      if(nrc==100)
          Trimite(client, "Serverul este plin, incercati mai tarziu\n", 41);
      else
      {
	  nrc++;
	  clienti[nrc]=client;
	  ReincarcaLista(client,cmd_azi,0);
          /* s-a realizat conexiunea, se astepta mesajul */
	  td=(struct thData*)malloc(sizeof(struct thData));	
	  td->idThread=i++;
	  td->cl=client;
      }
      pthread_create(&th, NULL, &treat, td);	      
  }				/* while */
}			/* main */
int status_xpath(xmlNodeSetPtr nodes, int opt)
{
    xmlNodePtr cur;
    char r[10];
    bzero(r,10);
    int size;
    int i;
    size = (nodes) ? nodes->nodeNr : 0;
    if(size)
    {
   	if(opt==1)
            strcat(r,"ajuns");
        else
            strcat(r,"plecat");
    }
    else 
        return 0;
    for(i = 0; i < size; ++i) {
	assert(nodes->nodeTab[i]);
	if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE) {
	    cur = nodes->nodeTab[i];   
   	}
   	xmlNodeSetContent(cur->children,(const xmlChar*) r);
    }
    return 1;
}
int print_xpath(xmlNodeSetPtr nodes, int fd)
{ 
    xmlNodePtr cur;
    xmlChar *val;
    char list[1000];
    bzero(list,1000);
    int size;
    int i;
    size = (nodes) ? nodes->nodeNr : 0;
    for(i = 0; i < size; ++i) {
	assert(nodes->nodeTab[i]);
	if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE) {
	    cur = nodes->nodeTab[i];
	    val = xmlNodeGetContent(cur->children);  
	    strcat(list,cur->name);
	    strcat(list,": ");
    	    if(val)
    	    {
    	        if((xmlStrcmp(cur->name, (const xmlChar *)"zi"))==0)
    	        {
    	            if((xmlStrcmp(val, (const xmlChar *)"0"))==0)
    	                strcat(list,"azi\n");
    	            else if((xmlStrcmp(val, (const xmlChar *)"-1"))==0)
    	                strcat(list,"ieri\n");
    	            else //==1, else nu avem functie care ar afisa
    	                strcat(list,"maine\n");
    	        }
    	        else if((xmlStrcmp(cur->name, (const xmlChar *)"sosire")==0) || (xmlStrcmp(cur->name, (const xmlChar *)"plecare")==0))
    	        {
    	            int l=xmlStrlen(val);
    	            for(int j=0;j<l;j++)
    	                if(val[j]=='.')
    	                {
    	                    val[j]=':';
    	                    break;
    	                }
    	            strcat(list,(char*)val);
    	        }
    	        
    	        else
    	            strcat(list,(char*)val);
    	    }
    	    strcat(list, "\n");
    	    if((xmlStrcmp(cur->name, (const xmlChar *)"zi"))==0)
    	        strcat(list,"\n");
    	    if(strlen(list)>900)
    	    {
    	       Trimite(fd,list,1000);
    	       bzero(list,1000);
    	    }
   	}
    }
    if(strlen(list))
    {
        Trimite(fd,list,1000);
        bzero(list,1000);
    }
    if(size) return 1;
    return 0;
}
int intarziere_xpath(xmlNodeSetPtr nodes, char * nr_min)
{ 
    xmlNodePtr cur;
    xmlChar *val;
    int size;
    int i;
    int fstatus=0;
    int zi=0;
    int h=GetTime();
    int m=h%100;
    h/=100;
    size = (nodes) ? nodes->nodeNr : 0;
    for(i=0;i < size; ++i) {
	assert(nodes->nodeTab[i]);
	if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE) {
	    cur = nodes->nodeTab[i];
	    val = xmlNodeGetContent(cur->children);  
	    if(((xmlStrcmp(cur->name, (const xmlChar *)"sosire"))==0) || ((xmlStrcmp(cur->name, (const xmlChar *)"plecare"))==0))
	    {
	        //timp estimat+=nr_min;
	        char *p;
	        p=strtok((char*)val,".");
	        int ora=atoi(p);
	        p=strtok(NULL,".");
	        int minut=atoi(p);
	        minut+=atoi(nr_min);
	        while(minut<0)
	        {
	            ora--;
	            minut+=60;
	        }
	        ora+=minut/60;
	        minut%=60;
	        char r[10];
	        bzero(r,10);
	        //verificam daca s-a schimbat ziua
	        if(ora>=24)
	        {
	            zi=1;
	            ora-=24;
	        }
	        if(ora<0)
	        {
	            zi=-1;
	            ora+=24;
	        }
	        int j=0;
	        if(ora>=10)
	            r[j++]=ora/10+'0';
	        r[j++]=ora%10+'0';
	        r[j++]='.';
	        r[j++]=minut/10+'0';
	        r[j++]=minut%10+'0';
	        if(h>ora || (h==ora && m>=minut))//a trecut noul timp
	        {
	            if((xmlStrcmp(cur->name, (const xmlChar *)"sosire"))==0)
	                fstatus=1;
	            else
	                fstatus=2;
	        }      
	        xmlNodeSetContent(cur->children,(const xmlChar*) r);
	    }
	    if((xmlStrcmp(cur->name, (const xmlChar *)"status"))==0)
	    {
	        char r[10];
	        bzero(r,10);
	        if(zi==1)
	        {
	            strcat(r,"urmeaza");
	            xmlNodeSetContent(cur->children,(const xmlChar*) r);
	        }
	        else if(zi==-1)
	        {
	            if((xmlStrcmp(cur->parent->name, (const xmlChar *)"PanaLa"))==0)
	            {
	                strcat(r,"ajuns");
	                xmlNodeSetContent(cur->children,(const xmlChar*) r);
	            }
	            else
	            {
	                strcat(r,"plecat");
	                xmlNodeSetContent(cur->children,(const xmlChar*) r);
	            }
	        }
	        else if(fstatus==0)
	        {
	            strcat(r,"urmeaza");
	            xmlNodeSetContent(cur->children,(const xmlChar*) r);
	        }
	        else if(fstatus==1)
	        {
	            strcat(r,"ajuns");
	            xmlNodeSetContent(cur->children,(const xmlChar*) r);
	        }
	        else if(fstatus==2)
	        {
	            strcat(r,"plecat");
	            xmlNodeSetContent(cur->children,(const xmlChar*) r);
	        }
	        fstatus=0;
	    }
	    if(((xmlStrcmp(cur->name, (const xmlChar *)"zi"))==0)&&(zi!=0))
	    {
	        char r[10];
	        bzero(r,10);
	        int x=atoi((char*)val), l=1;
	        if(zi==1)//oprirea e acum in ziua urmatoare
	            x++;
	        else//oprirea e acum in ziua anterioara
	            x--;
	        int j=0, y=x;
	        if(x<0)
	        {
	            r[j]='-';
	            j++;
	            x=(-1)*x;
	            y=x;
	        }
	        while(x)
	        {
	            l*=10;
	            x/=10;
	        }
	        if(l==1)
	            r[j]='0';
	        while(l>1)
	        {
	            r[j]=y%l/(l/10)+'0';
	            l/=10;
	            j++;
	        }
	        xmlNodeSetContent(cur->children,(const xmlChar*) r);
	        zi=0;
	    }
	    if((xmlStrcmp(cur->name, (const xmlChar *)"intarziere"))==0)
	    {
	        //val+=nr_min;
	        int x=atoi((char*)val),l=1;
	        x+=atoi(nr_min);
	        char r[10];
	        bzero(r,10);
	        int j=0,y=x;
	        if(x<0)
	        {
	            r[j]='-';
	            j++;
	            x=(-1)*x;
	            y=x;
	        }
	        while(x)
	        {
	            l*=10;
	            x/=10;
	        }
	        if(l==1)
	            r[j]='0';
	        while(l>1)
	        {
	            r[j]=y%l/(l/10)+'0';
	            l/=10;
	            j++;
	        }
	        xmlNodeSetContent(cur->children,(const xmlChar*) r);  
	    }
   	}
    }
    for(i = 0; i < size; ++i) 
    {
	assert(nodes->nodeTab[i]);
	if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE) 
	{
	    cur = nodes->nodeTab[i];
	    if(xmlStrcmp(cur->parent->name, (const xmlChar *)"DeLa"))//!=0
	    {
	        //intarzierea nu afecteaza toate opririle trenului => trebuie verificata validitatea
	        //oprire nr x dupa oprire nr y, any y<x
	        xmlNodePtr x,y,xday,yday;
	        if(xmlStrcmp(cur->name, (const xmlChar *)"sosire")==0) //ultima oprire
	        {
	            x=cur;
	            y=cur->parent->prev->prev->children->next->next->next->next->next;//ultima plecare neschimbata
	            yday=y->next->next->next->next->next->next->next->next; //ziua plecarii y
	            xday=x->next->next->next->next->next->next->next->next; //ziua sosirii x           
	        }
	        else //o oprire oarecare
	        {
	            x=cur->next->next;//e prima sosire schimbata
	            y=cur->parent->prev->prev;//(va fi) ultima plecare neschimbata
	            if(xmlStrcmp(y->name, (const xmlChar *)"DeLa")==0)
	                y=y->children->next;
	            else
	                y=y->children->next->next->next->next->next;
	            yday=y->next->next->next->next->next->next->next->next; //ziua plecarii y
	            xday=x->next->next->next->next->next->next->next->next->next->next; //ziua sosirii x
	        }
	        float valx=atof(xmlNodeGetContent(x->children));
	        float valy=atof(xmlNodeGetContent(y->children));
	        int valyday=atoi(xmlNodeGetContent(yday->children));
	        int valxday=atoi(xmlNodeGetContent(xday->children));
	        if(valxday<valyday) //zi x < zi y
	            return 2;
	        if((valxday==valyday) && (valx<valy)) //timp x < timp y
	            return 2;
	   }
	   break;
	}
    } 
    if(size) return 1;
    return 0;
}
int exec_xpath(const char* filename, const xmlChar* xpathExpr, int fd, char *  nr_min) 
{
    xmlDocPtr doc;
    xmlXPathContextPtr xpathCtx; 
    xmlXPathObjectPtr xpathObj; 
    assert(filename);
    assert(xpathExpr);
    int change=0;
    /* Load xml document */
    doc = xmlParseFile(filename);
    if (doc == NULL) {
	printf("Error: unable to parse file \"%s\"\n", filename);
	return(-1);
    }
    /* Cream contextul pt evaluaare xpath */
    xpathCtx = xmlXPathNewContext(doc);
    if(xpathCtx == NULL) {
        printf("Error: unable to create new XPath context\n");
        xmlFreeDoc(doc); 
        return(-1);
    }   
    /* Evaluam expresie xpath */
    xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx);
    if(xpathObj == NULL) {
        printf("Error: unable to evaluate xpath expression \"%s\"\n", xpathExpr);
        xmlXPathFreeContext(xpathCtx); 
        xmlFreeDoc(doc); 
        return(-1);
    }
    /* Rezultatele */
    if(fd)
        change=print_xpath(xpathObj->nodesetval, fd); //refolosesc variabila, dar change=1 daca au existat noduri de afisat, nu daca are loc o schimbare
    else
    {
        if(strcmp(nr_min," ")==0)
            change=status_xpath(xpathObj->nodesetval, 1);
        else if(strcmp(nr_min,"  ")==0)
            change=status_xpath(xpathObj->nodesetval, 2);
        else
            change=intarziere_xpath(xpathObj->nodesetval, nr_min);
        if(change==1)
            xmlSaveFile(filename,doc);
    }
    /* Cleanup */
    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx); 
    xmlFreeDoc(doc);    
    return change;
}
int ReincarcaLista(int fd, const xmlChar *command, char * nr_min)
{
  xmlInitParser();
  LIBXML_TEST_VERSION
  int change=exec_xpath(bd, command, fd, nr_min);
  if(change<0) {
	perror ("[server] Eroare la executia xpath\n");
	return 0;
  }
  xmlCleanupParser();
  if(fd&&(change==1))
  {
      char list[100];
      bzero(list,100);
      strcat(list,"Timpii sosire/plecare sunt cei estimati conform intarzierilor.\n");
      Trimite(fd,list,100);
  }
  if(fd&&(change==0))
  {
      char list[100];
      bzero(list,100);
      strcat(list,"Nu am gasit trenuri care se potrivesc cerintelor specificate.\n");
      Trimite(fd,list,100);
  }
  return change;
}
void Trimite(int fd, char *msgrasp, int bytes)
{
  if (bytes && write (fd, msgrasp, bytes) < 0)
      perror ("[server] Eroare la write() catre client.\n");
}
int GetTime()
{
  time_t clk = time(NULL);
  char *data_timp=ctime(&clk);
  char *p;
  p=strtok(data_timp," :");//p=ziua saptamanii
  for (int i=0;i<3;i++)//ajungem la p=ora
      p=strtok(NULL," :");
  int ora=atoi(p);
  p=strtok(NULL," :");//ajungem la p=minut
  int minut=atoi(p);
  //bd nu tine cont de secunde, deci nu are rost sa le extragem
  //printf("Ora curenta: %d:%d\n",ora,minut);
  ora=ora*100+minut;
  return ora;
}
void NextHour(int fd, char * var)
{
  //extragem ora si minutul curent real
  int ora=GetTime();
  int minut=ora%100;
  ora/=100;
  char c[10];
  bzero(c,10);
      //"//* [number(./sosire)<19 and number(./sosire)>18 and number(./zi)=0]/* | //* [number(./sosire)<19 and number(./sosire)>18 and number(./zi)=0] | //* [number(./sosire)<19 and number(./sosire)>18 and number(./zi)=0]/ancestor::* | //* [number(./sosire)<19 and number(./sosire)>18 and number(./zi)=0]/ancestor::tren/cod"
  char condition[1000]; //construim conditioa folosita in comanda
  bzero(condition,1000);
  
  int i=0;
  if(ora>=10)
      c[i++]=ora/10+'0';
  c[i++]=ora%10+'0';
  c[i++]='.';
  c[i++]=minut/10+'0';
  c[i++]=minut%10+'0';
  
  if(ora<23)
  {
      strcat(condition, "[number(./");
      strcat(condition, var);
      strcat(condition, ")>=");
      strcat(condition,c);
      strcat(condition," and number(./");
      strcat(condition, var);
      strcat(condition,")<=");
      ora++;
      i=0;
      if(ora>=10)
          c[i++]=ora/10+'0';
      c[i++]=ora%10+'0';
      c[i++]='.';
      c[i++]=minut/10+'0';
      c[i++]=minut%10+'0';
      strcat(condition,c);
      strcat(condition," and number(./zi)=0]");
  }
  else //(var>23.mm and zi=0) or (var<00.mm and zi=1)
  {
      strcat(condition, "[(number(./");
      strcat(condition, var);
      strcat(condition, ")>=");
      strcat(condition,c);
      strcat(condition, " and number(./zi)=0) or (number(./");
      strcat(condition, var);
      strcat(condition, ")<=");
      c[0]='0';
      c[1]='.';
      c[2]=minut/10+'0';
      c[3]=minut%10+'0';
      strcat(condition,c);
      strcat(condition, " and number(./zi)=1)]");
  }
  char command[1000]; //comanda completa pe care o apelam cu xpath
  bzero(command,1000);
  strcat(command,"//* ");
  strcat(command,condition);
  strcat(command,"/* | //* ");
  strcat(command,condition);
  strcat(command," | //* ");
  strcat(command,condition);
  strcat(command,"/ancestor::* | //* ");
  strcat(command,condition);
  strcat(command,"/ancestor::tren/cod");
  //afisam si og time, si +intarziere
  ReincarcaLista(fd,command,0);
}
int ProcesareCmd(int fd)
{
  int bytes;			/* numarul de octeti cititi/scrisi */
  char msg[1000];		//mesajul primit de la client 
  char msgrasp[1000]=" ";      //mesaj de raspuns pentru client
  bytes = read (fd, msg, 1000);
  if (bytes < 0)
    {
      perror ("Eroare la read() de la client.\n");
      return 0;
    }
  printf ("[server]Mesajul a fost receptionat...%s\n", msg);
  /*pregatim mesajul de raspuns */
  bzero(msgrasp,1000);  //pregatim variabila msgrasp (o golim)
  if(strncmp(msg,"help",4)==0)
  {
      strcat(msgrasp,"Comenzile acceptate sunt:\n help: afisare comenzi\n quit: inchidere client\n intarziere [cod_tren] [nr_min]: anuntarea unei intarzieri / nr_min poate fi negativ daca trenul a ajuns prea devreme\n lista_plecari: afiseaza plecarile din urmatoarea ora\n lista_sosiri: afiseaza sosirile din urmatoare ora\n refresh: cerere manuala de retrimitere a listei trenurilor de azi\n");
      Trimite(fd, msgrasp, bytes);
  }
  else if(strncmp(msg,"quit",4)==0)
  {
      strcat(msgrasp,"quit");
      Trimite(fd, msgrasp, bytes);
      for(int i=1;i<=nrc;i++)
          if(clienti[i]==fd)
          {
              clienti[i]=clienti[nrc];
              nrc--;
              break;
          }
      return bytes;
  }
  else if(strncmp(msg,"lista_plecari",13)==0)
      NextHour(fd,"plecare");
  else if(strncmp(msg,"lista_sosiri",12)==0)
      NextHour(fd,"sosire");
  else if(strncmp(msg,"intarziere",10)==0)
  {
      //verificare sintaxa (daca are [cod_tren] [nr_min]); incorect: trimite "Sintaxa:...";
      char *p;
      char c[10];
      bzero(c,10);
      p=strtok(msg," "); //p este "tren_intarziere"
      if((p=strtok(NULL," "))==0) //p NULL sau [cod_tren]
      {
          strcat(msgrasp,"Eroare:sintaxa incorecta\nSintaxa: tren_intarziere [cod_tren] [nr_min]\n");
          Trimite(fd, msgrasp, bytes);
          return 0;
      }
      //ne asiguram ca [cod_tren] are doar cifre
      int len=strlen(p);
      for(int i=0;i<len;i++)
          if(!isdigit(p[i]))
          {
              strcat(msgrasp,"Eroare:sintaxa incorecta: cod_tren introdus trebuie sa fie format doar din cifre\nSintaxa: intarziere [cod_tren] [nr_min]\n");
              Trimite(fd, msgrasp, bytes);
              return 0;
          } 
      char cod_tren[10];
      bzero(cod_tren,10);
      strcat(cod_tren,p); // dupa verificari, cod_tren sigur va fi preluat corect
      if((p=strtok(NULL," "))==0) //p NULL sau [nr_min]
      {
          strcat(msgrasp,"Eroare:sintaxa incorecta\nSintaxa: tren_intarziere [cod_tren] [nr_min]\n");
          Trimite(fd, msgrasp, bytes);
          return 0;
      }
      //ne asiguram ca [nr_min] are doar cifre
      len=strlen(p);
      //validarea nr_min in aceeasi maniera ca cod_tren ducea la erori (erau niste caractere nedorite la finalul tokenului)
      int i=0,ok=0;
      if(p[i]=='-')
          i++;
      while(isdigit(p[i]))
      {
          ok=1;
          i++;
      }
      if(!ok)
      {
          strcat(msgrasp,"Eroare:sintaxa incorecta: nr_min trebuie sa fie un numar (se ia in considerare doar nr dinainte primului caracter incorect)\nSintaxa: intarziere [cod_tren] [nr_min]\n");
          Trimite(fd, msgrasp, bytes);
          return 0;
      }
      char nr_min[10];
      bzero(nr_min,10);
      strncat(nr_min,p,i); // dupa verificari, nr_min sigur va fi preluat corect
      int ora=GetTime();
      int minut=ora%100;
      ora/=100;
      i=0;
      if(ora>=10)
          c[i++]=ora/10+'0';
      c[i++]=ora%10+'0';
      c[i++]='.';
      c[i++]=minut/10+'0';
      c[i++]=minut%10+'0';
        //"//tren [./cod='5']//* [(number(./sosire)>=15.28 or number(./plecare)>=15.28) and number(./zi)=0]/*
      //construim comanda
      char command[1000];
      bzero(command,1000);
      strcat(command,"//tren [./cod='");
      strcat(command,cod_tren);
      int temp=atoi(nr_min);
      temp=(-1)*temp;
      if(temp>=24*60)
      {
          Trimite(fd,"Va rugam sa nu trimiteti intarzieri de mai mult de o zi\n",56);
          return 0;
      }
      if(nr_min[0]=='-')
      {
          strcat(command,"']//* [((number(./sosire)>=");
          strcat(command,c);
          strcat(command,"or number(./plecare)>=");
          strcat(command,c);
          strcat(command,") and number(./zi)=0) or number(./zi)>=1]/*");
      }
      else
      {
          //(15.28-0.05) pt nr_min=5; if nr_min>time, luam toate opririle de azi
          int m=atoi(nr_min);
          if(m>=24*60)
          {
              Trimite(fd,"Va rugam sa nu trimiteti intarzieri de mai mult de o zi\n",56);
              return 0;
          }
          int h=m/60;
          m%=60;
          char r[10];
          bzero(r,10);
          i=0;
          if(minut<m)
              m+=40; //adaugam un nr artificial de min pt ca scaderea c-r (de mai jos) sa dea rezultatul corect in minute
          if(h>=10)
              r[i++]=h/10+'0';
          r[i++]=h%10+'0';
          r[i++]='.';
          r[i++]=m/10+'0';
          r[i++]=m%10+'0';
          if(ora>h || (ora==h && minut>=m))//(15.28-0.05) valid
          {
              strcat(command,"']//* [((number(./sosire)>=(");
              strcat(command,c);
              strcat(command,"-");
              strcat(command,r);
              strcat(command,")or number(./plecare)>=(");
              strcat(command,c);
              strcat(command,"-");
              strcat(command,r);
              strcat(command,")) and number(./zi)=0) or number(./zi)>=1]/*");
          }
          else //toate opririle de azi, maine + (24.00+c-r) ex: tren de la 23:55 ieri ajunge la 00:05 azi, e anuntata intarziere 10 min 
          {
              strcat(command,"']//* [((number(./sosire)>=(24.00+");
              strcat(command,c);
              strcat(command,"-");
              strcat(command,r);
              strcat(command,")or number(./plecare)>=(24.00+");
              strcat(command,c);
              strcat(command,"-");
              strcat(command,r);
              strcat(command,")) and number(./zi)=-1) or number(./zi)>=0]/*");
          }
      }
      int change=ReincarcaLista(0,command,nr_min);
      //actualizam lista pt toti clientii, daca au avut loc schimbari
      if(change==1)
          for(int i=1;i<=nrc;i++)
              ReincarcaLista(clienti[i],cmd_azi,0);
      else if(change==2)
          Trimite(fd,"Intarziere invalida, nu poti schimba ordinea opririlor!\n",56);
      else Trimite(fd,"Nu au avut loc schimbari\n",25);
  }
  else if(strncmp(msg,"refresh", 7)==0)
   	ReincarcaLista(fd,cmd_azi, 0);
  else
  {
      strcat(msgrasp,"Aceasta nu e o comanda acceptata. Pentru lista comenzilor, scrieti 'help'.\n");
      Trimite(fd, msgrasp, bytes);
  }
  return 0;
}
static void *treat(void * arg)
{		
	struct thData tdL= *((struct thData*)arg);
	int client=tdL.cl;
	
	pthread_detach(pthread_self());
	for( ; ; )
	{
	    printf ("[thread]- %d - Asteptam mesajul...\n", tdL.idThread);
	    fflush (stdout);
	    if(ProcesareCmd(client) )//procesarea cererii
	    {
	        /* am terminat cu acest client, inchidem conexiunea */
	        printf ("[server] S-a deconectat clientul cu descriptorul %d.\n",client);
	        fflush (stdout);
	        close (client);		/* inchidem conexiunea cu clientul */ 
	        return (NULL);	
	    }				
	}
}
static void *Refresh(void * arg)
{		
  struct thData tdL= *((struct thData*)arg);
  int i;
  int change,ora,minut;
  char c[10];
  char command[1000];
  pthread_detach(pthread_self());
 
  for( ; ; )
  {
      change=0;
      ora=GetTime();
      minut=ora%100;
      ora/=100;
      bzero(c,10);
      i=0;
      if(ora>=10)
          c[i++]=ora/10+'0';
      c[i++]=ora%10+'0';
      c[i++]='.';
      c[i++]=minut/10+'0';
      c[i++]=minut%10+'0';
      //"/*/*/* [number(./zi)=0 number(./sosire)<13 and ./status='urmeaza']/status" -> status=ajuns
      bzero(command,1000);
      strcat(command,"/*/*/* [number(./zi)=0 and number(./sosire)<");
      strcat(command,c);
      strcat(command," and ./status='urmeaza']/status");
      change+=ReincarcaLista(0,command," ");
      //"/*/*/* [number(./zi)=0 and number(./plecare)<14.44 and (./status='ajuns' or ( name()='DeLa' and ./status='urmeaza'))]/status"
      bzero(command,1000);
      strcat(command,"/*/*/* [number(./zi)=0 and number(./plecare)<");
      strcat(command,c);
      strcat(command," and (./status='ajuns' or ( name()='DeLa' and ./status='urmeaza'))]/status");
      change+=ReincarcaLista(0,command,"  ");
      if(change)
          for(i=1;i<=nrc;i++)
              ReincarcaLista(clienti[i],cmd_azi,0);
      sleep(60);
  }
}
