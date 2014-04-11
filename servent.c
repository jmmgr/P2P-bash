#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <sys/time.h>
#include <unistd.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>

#include <dirent.h>

#include <sys/stat.h>

#define WEBCACHEPORT 80
#define MYPORT 6346
#define MAX_MESSAGE 550
#define MAX_IPS 10
#define MAX_LONG_IP 22
#define MAX_HILOS 10
#define MAX_CONEXIONES 3
#define BLOCK 1024
#define RESP_OK 200
#define RESP_BUSY 503
#define TIMEOUT 30


int G_INTERFAZ = 1;
int G_ESCUCHAR = 1;
int G_RECIBIR = 1;
char* G_ROWEBCACHE = "http://ctel011/0057514/cgi-bin/gcache.cgi";
char* G_SHAREPATH = "share";
char* G_INCOMINGPATH = "incoming";
char* G_DOWNLOADPATH = "download";
char* G_LOCALPATH = ".";
int G_LOCALPORT = 6346;
fd_set master, readfds;


int lista_hilos[MAX_HILOS];
char ** lista_ips;
char ** conexiones;

int get_total_conexiones();
int socket_con[MAX_CONEXIONES];
int lista_sockets[MAX_IPS];
void parsear_ips(char* msj);
int respuesta_handshake(char* msg);
void get_ips_xtry(char* msg, char** xtry);
char* get_ip();
int get_num_conexiones();
int obtener_socket_libre();
int get_archivos(char * criterio);
void * escuchar();
void * handshake();
int re_handshake(char** ips_xtry);
void * interfaz();
void set_menu(char* param, char* valor);
void * recibir(void* socket);
int crear_query(char * criterio, char** output);
int crear_query_hit( char ** output,int number);
int decodificar_query_hit(char *query,int socket);
int short2leb(char *dest, unsigned short v);
unsigned short leb2short(char *orig);
unsigned int leb2int(char *orig);	
unsigned int int2leb(char *dest, int v);
int block_sending(int sock,char *path);
int block_recv(int sock, char * path,char * msg);
void mover_a_downloads(char * path);

int sockfd, sockaux, socket_escucha;
struct sockaddr_in my_addr, their_addr, listen_addr, wc_addr;

char ** nombres_compartidos;
char ** index_compartidos;
unsigned int * tamanios_compartidos;
char *archivo_recibir;

char nombres_hit[100][256];
char  index_hit[100][256];
char  ip_puerto_hit[100][MAX_LONG_IP];
unsigned int tamanios_hit[100];
int numero_query_hit=0;
int socket_hit[100];
int query_hits_recibidas=0;

pthread_t hilo_escucha, hilo_comunica, hilo_interfaz;
pthread_t hilo_conexiones[MAX_HILOS];
struct sockaddr_in lista_sockaddrs[MAX_IPS];

int main() {
	FILE *file = fopen("rotella.cfg","r");
	
	char** comandos = (char **)calloc(5, 200);
	char* conf = (char*)malloc(1000);
	int lec = 0;
	char c;
	char * delim = "\r\n";
	char * result =(char*)malloc(MAX_MESSAGE);
	int a = 0;
	char* iploc = get_ip();
	char* msg2;
	char msg[MAX_MESSAGE] = "GET http://ctel011/0057514/cgi-bin/gcache.cgi?ip=";
	char msgaux2 [] = " HTTP/1.1\r\nHost: ctel011\r\n\r\n";
	char puerto[6];
	char* resp1;
	char* resp2;
	int nbytes;
	if(file != NULL){
	while(1) {     /* keep looping... */
	      c = fgetc(file);
	      if(c!=EOF) {
	      	conf[lec++]= c;
		printf("%c", c);  
		/* print the file one character at a time */
	      }
	      else {
		break;     /* ...break when EOF is reached */
	      }
    	}

	printf("conf: %s\n",conf);
	
	result= strtok(conf,delim);
	lec =0;
	while(result != NULL){
		comandos[lec]= result;
		lec++;
		result = strtok(NULL, delim);
	}
	lec=0;
	while(comandos[lec] != NULL){
		int lec2 =0;
		char** comando = (char**)calloc(200, sizeof(char));
		char* param = comando[1];
		printf("auxLista %d %s\n", lec, comandos[lec]);
		
		
		result = strtok(comandos[lec], " ");
		while(result != NULL){
			comando[lec2++]=result;
			result = strtok(NULL," ");
		
		}
		
		printf("param %s\n", param);
		set_menu(comando[1], comando[2]);
		lec++;
		}
	}
	
	
	
	/* Hasta aqui probamos */
	
	for (a = 0; a <= MAX_CONEXIONES; a++){
		socket_con[a] = -1;
	}
	
	printf("introduzca puerto\n");
	printf("Conectandose a la WEBCACHE\n");
	printf("iploc = %s \n", iploc);
	msg2 = (char*)malloc(MAX_MESSAGE);
	memset(msg2, 0, sizeof(msg2));
	msg2 = strcpy(msg2, "GET ");
	strcat(msg2, G_ROWEBCACHE);
	strcat(msg2, "?hostfile=1 HTTP/1.1\r\nHost: ctel011\r\n\r\n");
	printf("%s\n",msg2);
	strcat(msg, iploc);
	strcat(msg, ":");
	
	sprintf(puerto, "%d", G_LOCALPORT);
	strcat(msg, puerto);
	strcat(msg, msgaux2);
	lista_ips = (char **)calloc(MAX_IPS,MAX_LONG_IP);
	conexiones = (char **)calloc(MAX_CONEXIONES, MAX_LONG_IP);
	resp1 = (char *)malloc(MAX_MESSAGE);
	memset(resp1, 0, sizeof(resp1));
	resp2 = (char *)malloc(MAX_MESSAGE);

	/* Chequear si devuelve -1! */
		
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(-1);
	}
	wc_addr.sin_family = AF_INET; /* Ordenación de la máquina */
	wc_addr.sin_port = htons(WEBCACHEPORT); /* Ordenación de la red */
	wc_addr.sin_addr.s_addr = inet_addr(get_ip());
	/* Pone a cero el resto de la estructura */
	memset(&(my_addr.sin_zero), '\0', 8);
	/* Chequear si devuelve -1! */
	if(connect(sockfd, (struct sockaddr *)&wc_addr, sizeof(struct sockaddr_in))<0){
		perror("connect");
		exit(-1);
	}
	if((nbytes= send(sockfd, (char *)msg2, strlen(msg2), 0))<0){
		perror("send");
		exit(-1);
	}
	if ((nbytes= recv(sockfd, (char *) resp1, MAX_MESSAGE, 0))<0){
		perror("recv");
		exit(-1);
	}
	
	parsear_ips(resp1);
	if(send(sockfd, (char *)msg, strlen(msg), 0)<0){
		perror("send");
		exit(-1);
	}
	
	if ((nbytes=recv(sockfd, (char *) resp2, MAX_MESSAGE, 0))<0){
		perror("recv");
		exit(-1);
	}
	if (close(sockfd)<0){
		perror("close");
		exit(-1);
	}
	printf("Conectado a la WEBCACHE\nHacemos el handshake \n");
	if(pthread_create( &hilo_comunica, NULL, handshake, NULL)!=0)
	{
		perror("pthread_create");
		exit(-1);
	}
	pthread_join(hilo_comunica,NULL);
	if(pthread_create( &hilo_escucha, NULL, escuchar, NULL)!=0)
	{
		perror("pthread_create");
		exit(-1);
	}

	if(pthread_create(&hilo_interfaz,NULL, interfaz,NULL)!=0)
	{
		perror("pthread_create");
		exit(-1);
	}

	pthread_join(hilo_interfaz,NULL);
	pthread_join(hilo_escucha, NULL);
	printf("main: padre: hilo terminado\n");
	
	free(resp1);
	free(resp2);
	
	printf("adios!\n");

	return 1;
}
void set_menu(char* param, char* valor){
	if( strcasecmp(param, "rowebcache") == 0){
		printf("G_ROWEBCACHE antes %s\n",G_ROWEBCACHE);
		G_ROWEBCACHE = (char*)malloc(strlen(valor));
		strcpy(G_ROWEBCACHE, valor);
		printf("G_ROWEBCACHE despues %s\n",G_ROWEBCACHE);
	}else if(strcasecmp(param, "localport") == 0){
		printf("G_LOCALPORT antes %d\n", G_LOCALPORT);
		G_LOCALPORT = atoi(valor);
		printf("G_LOCALPORT despues %d\n", G_LOCALPORT);
	}else if(strcasecmp(param, "sharepath") == 0){
		printf("G_SHAREPATH antes %s\n",G_SHAREPATH);
		G_SHAREPATH = (char*)malloc(strlen(valor));
		strcpy(G_SHAREPATH, valor);
		printf("G_SHAREPATH despues %s\n",G_SHAREPATH);
	}else if(strcasecmp(param, "incomingpath") == 0){
		printf("G_INCOMING antes %s\n",G_INCOMINGPATH);
		G_INCOMINGPATH = (char*)malloc(strlen(valor));
		strcpy(G_INCOMINGPATH, valor);
		printf("G_INCOMING despues %s\n",G_INCOMINGPATH);
	}else if(strcasecmp(param, "downloadpath") == 0){
		printf("G_DOWNLOADPATH antes %s\n",G_DOWNLOADPATH);
		G_DOWNLOADPATH = (char*)malloc(strlen(valor));
		strcpy(G_DOWNLOADPATH, valor);
		printf("G_DOWNLOADPATH despues %s\n",G_DOWNLOADPATH);
	}


}
int obtener_socket_libre(){
	int z = 0;
	for (z = 0; z < MAX_CONEXIONES ; z++){
		if(socket_con[z] == -1)
		{
			return z; 
		}
	}
	return MAX_CONEXIONES;

}

int get_total_conexiones(){
int z=0,i=0;
	for (z = 0; z < MAX_CONEXIONES ; z++){
		if(socket_con[z]!=-1 )
		i++;
	}
return i;
}

void * escuchar(){
	int conex_libre = obtener_socket_libre();
	char* resprot1 = (char*) malloc(MAX_MESSAGE);
	char* resprot2 = (char*) malloc(MAX_MESSAGE);
	char* msgrot1 = "ROTELLA/0.1 200 OK\r\nUser-Agent: Rotella Client\r\n\r\n";
	char msgbusy[MAX_MESSAGE] = "ROTELLA/0.1 503 BUSY\r\nUser-Agent: Rotella Client\r\nX-Try: ";
	int nbytes = 0;
	unsigned int sin_size;
	unsigned short aux51 ;
	fd_set descriptoresLectura;
	socket_escucha = -1;
	sockaux = -1;
	if ((sockaux= socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(-1);
		}
	listen_addr.sin_family = AF_INET; /* Ordenación de la máquina */
	listen_addr.sin_port = htons(G_LOCALPORT);
	listen_addr.sin_addr.s_addr = inet_addr(get_ip());
	memset(&(listen_addr.sin_zero), '\0', 8);
	if ( bind( sockaux, (struct sockaddr *)&listen_addr, sizeof(struct sockaddr_in)) < 0){
		perror("bind");
		exit(-1);
		}
	while(G_ESCUCHAR == 1){
		struct timeval timeout;
		/* Timeout for select */
		timeout.tv_sec = TIMEOUT; 
		if(listen(sockaux, 5)<0){
			perror("listen");
			exit(-1);
		}
		sin_size = sizeof(struct sockaddr_in);
		FD_ZERO (&descriptoresLectura);
		FD_SET (sockaux, &descriptoresLectura); 
		select (sockaux+1, &descriptoresLectura, NULL, NULL,&timeout);
		if (FD_ISSET(sockaux, &descriptoresLectura)){
		if ((socket_escucha = accept(sockaux, (struct sockaddr *)&their_addr, &sin_size)) < 0){
			perror("accept");
			exit(-1);
		}else{

		}
		aux51 = ntohs(their_addr.sin_port);
			
		if ((nbytes=recv(socket_escucha, (char *)resprot1, MAX_MESSAGE, 0))<0){
			perror("recv");
			exit(-1);
		}	
		
		/*Comprobamos si estamos completos*/
		
		if(conex_libre < MAX_CONEXIONES){
			char ipCon[MAX_LONG_IP]="";
			char* ipConAux = inet_ntoa(their_addr.sin_addr);
			char aux52[6];
			socket_con[conex_libre]= socket_escucha;
			printf("Conectado a la ip: %s, en el escucha\n",ipConAux);
			strcat(ipCon, ipConAux);
			strcat(ipCon, ":");
			sprintf(aux52, "%d", aux51);
			strcat(ipCon, aux52);
			conexiones[conex_libre] = ipCon;
			if(send(socket_escucha, (char *)msgrot1, strlen(msgrot1), 0)<0){
				perror("send");
				exit(-1);
			}
			if ((nbytes=recv(socket_escucha, (char *) resprot2, MAX_MESSAGE, 0))<0){
				perror("recv");
				exit(-1);
			}
			if(pthread_create(&hilo_conexiones[conex_libre], NULL, recibir, (void *)socket_con[conex_libre])!=0)
			{
				perror("pthread_create");
				exit(-1);
			}
			conex_libre = obtener_socket_libre();
		
		}
		else{
			int x;
			for(x =0; x< conex_libre; x++){
			strcat(msgbusy, conexiones[x]);
			strcat(msgbusy, ",");
			}
			
			strcat(msgbusy,"\r\n\r\n");
			if(send(socket_escucha, (char *)msgbusy, strlen(msgbusy), 0)<0){
				perror("send");
				exit(-1);
			}
			strncpy(msgbusy, "ROTELLA/0.1 503 BUSY\r\nUser-Agent: Rotella Client\r\nX-Try: ", MAX_MESSAGE);
		
		}
	}
	}
	printf("hilo escuchar termina\n");
	return 0;
}


int get_num_conexiones(){
	int z = 0;
	for (z = 0; z < MAX_CONEXIONES ; z++){
		if(conexiones[z] == NULL)
		{
			return z; 
		}
	}
	return MAX_CONEXIONES;

}


void * handshake(){
	int i = 0;
	int j;
	int y;
	int nbytes=0;
	int conectado = 0;
	int respHS;
	char* msgrot1 = "ROTELLA CONNECT/0.1\r\nUser-Agent: Rotella Client\r\n\r\n";
	char* resprot1 = (char*)malloc(MAX_MESSAGE);
	char* msgrot2 = "ROTELLA/0.1 200 OK\r\n\r\n";
	char * result =NULL;
	char ** Ip_puerto;
	int conex_libre = 0;
	char ipAUX[MAX_LONG_IP];
	Ip_puerto = (char **)calloc(2,16);
	conex_libre = obtener_socket_libre();
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(G_LOCALPORT);
	my_addr.sin_addr.s_addr = inet_addr(get_ip());
	while(lista_ips[i] != NULL && conex_libre < MAX_CONEXIONES){
				
		if ((lista_sockets[i]= socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(-1);
		}
		/*separamos ip y puerto*/
		j=0;
		
		memcpy(ipAUX, lista_ips[i], MAX_LONG_IP);
		result= strtok(ipAUX,":");
		Ip_puerto[1]="3565";
		while(result != NULL){
			Ip_puerto[j]= result;
			j++;
			result = strtok(NULL, ":");
		}
		if(strlen(Ip_puerto[1])==0)
			Ip_puerto[1]="6346";
		
		lista_sockaddrs[i].sin_family = AF_INET; /* Ordenación de la máquina */
		lista_sockaddrs[i].sin_port = htons(atoi(Ip_puerto[1])); /* Ordenación de la red */
		lista_sockaddrs[i].sin_addr.s_addr = inet_addr(Ip_puerto[0]);
		/* Pone a cero el resto de la estructura */
		memset(&(lista_sockaddrs[i].sin_zero), '\0', 8);
		if((conectado = connect(lista_sockets[i], (struct sockaddr *)&lista_sockaddrs[i], sizeof(struct 	sockaddr_in)))<0){
			conectado=-1;
			perror("connect");
		}
/*	if(conectado<0)
		printf("handshake: conexion a %s:%s RECHAZADA%d\n",lista_ips[i],Ip_puerto[1],conectado);*/
	
	if(conectado >= 0){
		socket_con[conex_libre] = lista_sockets[i];
		conexiones[conex_libre] = lista_ips[i];
		
		printf("handshake: conexion a %s ACEPTADA\n",lista_ips[i]);
		if((nbytes= send(lista_sockets[i], (char *)msgrot1, strlen(msgrot1), 0))<0){
			perror("send");
			/*exit(-1);*/		}
		if ((nbytes= recv(lista_sockets[i], (char *) resprot1, MAX_MESSAGE, 0))<0){
			perror("recv");
			/*exit(-1);*/
		}
		respHS = respuesta_handshake(resprot1);
		if(respHS == RESP_OK){
			int IAUX=0;
			while(conexiones[IAUX] != NULL){
			IAUX++;
			}
			IAUX=0;
			while(socket_con[IAUX] > -1){
			IAUX++;
			}
			if(send(lista_sockets[i], (char *)msgrot2, strlen(msgrot2), 0)<0){
			perror("send");
			}
			if(pthread_create( &hilo_conexiones[conex_libre], NULL, recibir, (void *)socket_con[conex_libre])!=0)
			{
				perror("pthread_create");
				exit(-1);
			}
		conex_libre = obtener_socket_libre();
		}else if(respHS == RESP_BUSY){
			
			char** ips_xtry = (char **)calloc(MAX_CONEXIONES, MAX_LONG_IP);
			get_ips_xtry(resprot1, ips_xtry);
			
			for (y = 0; y < MAX_CONEXIONES; y++){
			}
			if (close(lista_sockets[i])<0){
				perror("close");
				exit(-1);
			}
			re_handshake(ips_xtry);
		}else{
			if (close(lista_sockets[i])<0){
				perror("close");
				exit(-1);
			}
		}
	}else{
		if (close(lista_sockets[i])<0){
			perror("close");
			exit(-1);
		}
	}

	i++;
	}
	return 0;
}
int re_handshake(char** ips_xtry){
	
	char* msgrot1 = "ROTELLA CONNECT/0.1\r\nUser-Agent: Rotella Client\r\n\r\n";
	char* resprot1 = (char*)malloc(MAX_MESSAGE);
	char* msgrot2 = "ROTELLA/0.1 200 OK\r\n\r\n";
	char * result =NULL;
	char ** Ip_puerto;
	int conex_libre = 0;
	int i=0;
	int conectado = 0;
	int j=0;
	char ipAUX[MAX_LONG_IP];
	int nbytes=0;
	int respHS;
	Ip_puerto = (char **)calloc(2,16);
	
	while(ips_xtry[i] != NULL){
		i++;
	}
	
	i=0;
	while(ips_xtry[i] != NULL){
		if(conex_libre < MAX_CONEXIONES){
			if ((lista_sockets[i]= socket(AF_INET, SOCK_STREAM, 0)) < 0) {
				perror("socket");
				exit(-1);
			}
			
			memcpy(ipAUX, ips_xtry[i], MAX_LONG_IP);
			result= strtok(ipAUX,":");
			Ip_puerto[1]="3565";
			while(result != NULL){
				Ip_puerto[j]= result;
				j++;
				result = strtok(NULL, ":");
			}
			if(strlen(Ip_puerto[1])==0)
				Ip_puerto[1]="6346";
			lista_sockaddrs[i].sin_family = AF_INET; /* Ordenación de la máquina */
			lista_sockaddrs[i].sin_port = htons(atoi(Ip_puerto[1])); /* Ordenación de la red */
			lista_sockaddrs[i].sin_addr.s_addr = inet_addr(Ip_puerto[0]);
			/* Pone a cero el resto de la estructura */
			memset(&(lista_sockaddrs[i].sin_zero), '\0', 8);
			if((conectado = connect(lista_sockets[i], (struct sockaddr *)&lista_sockaddrs[i], sizeof(struct 	sockaddr_in)))<0){
				conectado=-1;
				perror("connect");
			}
			if(conectado > -1){
				
				socket_con[conex_libre] = lista_sockets[i];
				conexiones[conex_libre] = ips_xtry[i];
				
				if((nbytes= send(lista_sockets[i], (char *)msgrot1, strlen(msgrot1), 0))<0){
					perror("send");
					/*exit(-1);*/				}
				if ((nbytes= recv(lista_sockets[i], (char *) resprot1, MAX_MESSAGE, 0))<0){
					perror("recv");
					/*exit(-1);*/
				}
				respHS = respuesta_handshake(resprot1);
				if(respHS == RESP_OK){
	
					if(send(lista_sockets[i], (char *)msgrot2, strlen(msgrot2), 0)<0){
						perror("send");
					}
					if(pthread_create( &hilo_conexiones[conex_libre], NULL, recibir, (void *)socket_con[conex_libre])!=0)
					{
						perror("pthread_create");
						exit(-1);
					}
					conex_libre = obtener_socket_libre();

				}else{
					if (close(lista_sockets[i])<0){
						perror("close");
						exit(-1);
					}
				}
			
			}else{
				if (close(lista_sockets[i])<0){
						perror("close");
						exit(-1);
				}
			}
		}
	i++;
	}
	return 0;
}

int respuesta_handshake(char* msg){

	char resAux[3];
	int res;
	resAux[0] = msg[12];
	resAux[1] = msg[13];
	resAux[2] = msg[14];
	res = atoi(resAux);
	
	return res;
}

void get_ips_xtry(char* msg, char** xtry){

	char** auxLista = (char **)calloc(MAX_CONEXIONES, MAX_LONG_IP);
	char* pch = strstr(msg, "X-Try:");
	char * result = strtok(pch, ",");
	int i =0;
	int j =0;
	pch = pch+7;
	
	while(result != NULL && strlen(result)>6){
		auxLista[i++] = result;
		result = strtok(NULL, ",");
	
	}
	for(j = 0; j < i; j++){
		xtry[j] = auxLista[j];
	}
	printf("get_ips_xtry: %s\n", pch);
	
	
}
void parsear_ips(char* msj){
	char ** auxListaIp;
	int i=0;
	int j=0;
	char* pch = strstr(msj, "\r\n\r\n");
	char * delim = "\r\n";
	char * result =NULL;
	auxListaIp = (char **)calloc(MAX_IPS,MAX_LONG_IP);
	result= strtok(pch,delim);
	while(result != NULL){
		auxListaIp[j]= result;
		j++;
		result = strtok(NULL, delim);
	}
	
	for(i=0;i<MAX_IPS;i++){
		if(auxListaIp[i+1] != NULL)
		{	
			if(auxListaIp[i+1][0] != ('0') )
			lista_ips[i] = auxListaIp[i+1];
		}
		
	}
free(auxListaIp);
}

void * interfaz(){
	
	while(G_INTERFAZ == 1){
		char* comando;
		char * palabra;
		int identificador=0,longitud=0;
		int i = 0;
		char * query;
		char * request;
		comando = (char *) malloc(256);
		palabra =(char *) malloc(256);
		
		
		printf("interfaz: introduzca comando\n");
		scanf("%s",comando);
		if (strcmp(comando,"find")==0)
		{
			query_hits_recibidas=0;
			printf("Buscamos archivo\n");
			
			numero_query_hit=0;
			scanf("%s",palabra);
			longitud=crear_query(palabra, &query);
			for(i=0; i< MAX_CONEXIONES; i++){
				if(socket_con[i]>-1){
				if(send(socket_con[i], (char *)query, longitud, 0)<0){
					perror("send");
				}
				}
			}
			
		}
		if (strcmp(comando,"get")==0)
		{
			
			request =(char*)malloc(MAX_MESSAGE);
			scanf("%d",&identificador);
			identificador--;
			strcat(request,"GET ");
			strcat(request,(char *)nombres_hit+identificador*256);
			strcat(request, "HTTP/1.1\r\nUser-Agent: ROtella\r\nHost: ");
			strcat(request,(char *)ip_puerto_hit+ MAX_LONG_IP*identificador);
			strcat(request,"\r\n Connection: close\r\n\r\n");
			if(send(socket_hit[identificador], (char *)request, strlen(request), 0)<0){
				perror("send");
			}
			archivo_recibir=(char*)malloc(256);
			strcpy(archivo_recibir,(char *)nombres_hit+identificador*256);
			
			
		}
		if (strcmp(comando,"stop")==0)
		{
			scanf("%d",&identificador);
					
		}
		if (strcmp(comando,"exit")==0){
		
		int conex_libre = obtener_socket_libre();
		
		G_ESCUCHAR = 0;
		G_RECIBIR = 0;
		G_INTERFAZ = 0;
		printf("Cerrando programa espere un momento\n");
		for(i = 0; i < conex_libre; i++){
			if (close(socket_con[i])<0)
			{
				perror("close");
			}
				
		
		}
		if (close(sockaux)<0)
		{
			perror("close");
		}
				
		break;
		}
		
		if (strcasecmp(comando,"set") == 0){
			char* p = (char*)malloc(20);
			char* v = (char*)malloc(200);
			scanf("%s", p);
			scanf("%s", v);
			printf("p: %s v: %s\n",p, v);
			set_menu(p,v);
		}
		}
/*liberamos las variables locales*/free(lista_ips);
free(conexiones);
free(nombres_compartidos);
free(index_compartidos);
free(tamanios_compartidos);
printf("hilo interfaz termina\n");
return 0;
}


int crear_query_hit( char**output,int numberOfhits)
{
/*0       Number of Hits. 
1-2     Port
3-6     IP Address
7-10    Speed The speed (in kb/second) of the responding host.
11-     Result Set.         
        Bytes:  Description:
        0-3     File Size. 
        4-      File Index.      
        x-      File Name. 
Last 16 Servent Identifier.
	
*/	int i=0,j=0,k=0;
	
	int longitudPayload=0,longitud_query;
	char puerto[2];
	char * ip=get_ip();
	char  payload[4];
	char * query;
	unsigned long aux_ip;
	unsigned long * ptr;
	unsigned char * puntero;
	sprintf(puerto,"%d",G_LOCALPORT);
	for(i=0;i<numberOfhits;i++)
		{
		char auxIndex[3]="";
		longitudPayload+=strlen((char*)nombres_compartidos+256*i); /*le sumamos lo que ocupa cada nombre de archivo*/
		longitudPayload+=strlen((char *)index_compartidos+256*i)-1;/*le sumamos lo que ocupan los index*/
		sprintf((char *)auxIndex,"%d",i+1);
		longitudPayload+=strlen(auxIndex);		
		}	
	
	longitudPayload = longitudPayload + (numberOfhits * 6); /*le sumamos bytes adicionales de cada archivo*/
	longitudPayload = longitudPayload +11;	/*le sumamos la cabecera de query hits*/
	longitud_query= longitudPayload + 23;
	/*char query[longitud_query];*/
	query = (char *)malloc(longitud_query);
	for(i=0;i<4;i++)
		payload[i]=0x00;
	
	for(i=0;i<16;i++)
	{
		*(query+i)=0x00;
	}
	*(query+16)=(char)0x81;
	*(query+17)=0x00;
	*(query+18)=0x00;
	*(query+22)=(char)payload[0];
	*(query+21)=(char)payload[1];
	*(query+20)=(char)payload[2];
	*(query+19)=(char)payload[3];
	*(query+23)=(char) numberOfhits;
	/*insertamos el puerto*/
	short2leb(puerto,G_LOCALPORT);
	for(i=0;i<2;i++)
		*(query+i+24)=puerto[i];
	/*insertamos la ip*/
	aux_ip = inet_addr(ip);	
	
	ptr =  &aux_ip;
	puntero =(unsigned char*) ptr;	
	for(i=0;i<4;i++)
	{	
		*(query+i+26)=(unsigned char) *puntero;
		puntero++;
		
	}
	for(i=0;i<4;i++)
		*(query+i+30)=0x00;
	j=34;
	
	for(i=0;i<numberOfhits;i++)
	{
		char tamanio[4];
		char * puntero1;
		int len;
		for(k=0;k<4;k++)
			tamanio[k]=0x00;
		int2leb(tamanio,(unsigned int)*(tamanios_compartidos+i));

		*(query+j+3)=tamanio[0];
		*(query+j+2)=tamanio[1];
		*(query+j+1)=tamanio[2];
		*(query+j)=tamanio[3];
		j +=4;
		puntero1 =(char*) index_compartidos+256*i;
		len = strlen((char*)index_compartidos+256*i);
		k=0;
		while(*(puntero1+k)!=0)
		{	
			*(query+j+len-k-1)=(char)*(puntero1+k);
			k++;
		}	
		j+=len;
		*(query+j)=0x00;
		j+=1;
		puntero1=NULL;
		len=strlen((char *)nombres_compartidos+i*256);
		puntero1=(char *) nombres_compartidos+i*256;
		for(k=0;k<len;k++)
		{
			*(query+j+k)=(char)*(puntero1+k);
		}
		*(query+j+k)=0x00;		
		j+=strlen((char *)nombres_compartidos+i*256)+1;
			
	}
	(*output) = (char *)malloc(longitud_query);
	memcpy(*output, query,longitud_query);
return longitud_query;
}


int decodificar_query_hit(char * msg,int sock){
	int numero_hits=(int)msg[23];
	char  ip[MAX_LONG_IP]="";
	int i=0,k=0,j=0;
	/*char tamanios[numero_hits][4];
	char nombres [numero_hits][256];
	char index [numero_hits][256];*/
	char ** tamanios;
	char **nombres;
	char ** index;
	char aux_puerto[2];
	unsigned short puerto;
	char puerto_string[10];
	char * puntero;
	tamanios=(char**)calloc(numero_hits,4);
	nombres=(char **)calloc(numero_hits,256);
	index=(char **)calloc(numero_hits,256);
	aux_puerto[0]=msg[24];
	aux_puerto[1]=msg[25];
	puerto = leb2short(aux_puerto);
	for(i=0;i<4;i++)
	{	
		int aux=msg[26+i];
		char numero[3]="";
		if(aux<0) aux+=256;
		sprintf(numero,"%d",aux);
		strcat((char*)ip,numero);
		if(i<3) strcat(ip,".");
		}
	strcat(ip,":");
	sprintf(puerto_string,"%d",puerto);
	strcat(ip,puerto_string);
	k=34;
	i=0;
	while(i<numero_hits)
	{
		int len=0;
		puntero=(char *)tamanios +i*4;
		for(j=0;j<4;j++)
		{			
			*(puntero+j)=(char)msg[k+3-j];
		}
		k+=4;
		j=0;
		len =strlen((char*)msg+k);
		strcpy((char*)index + 256*i,(char*)msg+k);
		k+=len+1;
		len = strlen((char*)msg+k);
		strcpy((char*)nombres + 256*i,(char*)msg+k);
		k+=len +1;
		i++;
	}
	for(i=0;i<numero_hits;i++)
	{
		strcpy((char*) index_hit + 256*numero_query_hit,(char*)index + 256*i);
		strcpy((char*) nombres_hit + 256*numero_query_hit,(char*)nombres + 256*i);
		tamanios_hit[numero_query_hit]=	leb2int((char *)tamanios + 4*i);	
		strcpy((char*)  ip_puerto_hit+ MAX_LONG_IP*numero_query_hit,ip);
		socket_hit[numero_query_hit]= sock;
		numero_query_hit++;		
	}
		
return numero_hits;
}

int crear_query(char * criterio, char**output)
{
	/*
  	Bytes:  Description:
   	0-15    Message ID/GUID (Globally Unique ID)
   	16      Payload Type
  	17      TTL (Time To Live)
 	18      Hops
 	19-22   Payload Length
 	0-1     Minimum Speed.        
	2-      Search Criteria. This field is terminated by a NUL (0x00).
	*/
	int longitud=0;
	int longitud_query=0;
	char  payload[4];
	int i=0;
	char * query;
	for(i=0;i<4;i++)
		payload[i]=0x00;
	longitud= 4 +  strlen(criterio);
	sprintf((char *)payload,"%d",longitud);
	longitud_query= 22+longitud;
	query=(char*)malloc(longitud_query);
	for(i=0;i<16;i++)
	{
		*(query +i) =(char)0x00;
	}
	*(query +16)=(char)0x80;
	*(query +17)=0x00;
	sprintf((char *)payload,"%d",longitud);
	*(query +18)=0x00;
	*(query +22)=(char)payload[0];
	*(query +21)=(char)payload[1];
	*(query +20)=(char)payload[2];
	*(query +19)=(char)payload[3];
	*(query +23)=0x00;
	*(query +24)=0x00;
	
	for(i=0;i<(int)strlen(criterio);i++)
	{
	*(query +25+i)=(char)criterio[i];
		}
	*(query +25+i)=0x00;
	(*output) = (char *)malloc(longitud_query);
	memcpy(*output, query,longitud_query);
return longitud_query;
}

int get_archivos(char * criterio){

	struct dirent ** archivos=NULL;
	struct stat  archivo;	
	int coincidencias =0;
	int numeroResultados;
	char ** aux_nombre;
	unsigned int * aux_index; 
	int i,j;
	numeroResultados=scandir(G_SHAREPATH,&archivos,NULL,NULL);
	aux_index =(unsigned int*)calloc(numeroResultados,sizeof(unsigned int));
	aux_nombre= (char **) calloc(numeroResultados, 256);
	
	for(i=0;i<numeroResultados;i++)
	{	char* nombre;
		nombre = (char *)malloc(256);
		strcpy(nombre, (char*)archivos[i]->d_name);
		if(strstr(nombre,criterio)!=NULL)
		{
			strcpy((char *)aux_nombre+i*256,nombre);
			*(aux_index + i)=archivos[i]->d_ino;
			coincidencias++;
		}
	}
	nombres_compartidos = (char **) calloc(coincidencias, 256);
	index_compartidos = (char**)calloc(coincidencias,256);
	tamanios_compartidos = (unsigned int *) calloc(coincidencias, sizeof(unsigned int));
	j=0;
	for(i=0;i<numeroResultados;i++)
	{
		if(strlen((char *)aux_nombre+ i *256)>0)
		{
		char index [20];
		char  aux[256]="";
		strcat(aux,G_SHAREPATH);
		strcat(aux,"/");
		strcat(aux,(char *)aux_nombre + i*256);
		strcpy((char*)nombres_compartidos + j*256,(char*)aux_nombre +i*256);
		stat(aux, &archivo);
		tamanios_compartidos[j]=archivo.st_size;
		sprintf(index,"%d",aux_index[i]);
		strcpy((char*) index_compartidos+256*j,index);
		j++;
		}
			
	}
	
return coincidencias;
}


void * recibir(void* sock){
	
	while(G_RECIBIR == 1){
	int socket = (int)sock;
	struct timeval timeout;  /* Timeout for select */
	char * msg;
	int nbytes,i=0;
	fd_set descriptoresLectura;
	msg= (char*) malloc(MAX_MESSAGE);
	
	timeout.tv_sec = TIMEOUT;
	
	FD_ZERO (&descriptoresLectura);
	FD_SET (socket, &descriptoresLectura); 
	select (socket+1, &descriptoresLectura, NULL, NULL,&timeout);
	if (FD_ISSET(socket, &descriptoresLectura)){
	if ((nbytes=recv(socket, (char *) msg, MAX_MESSAGE, 0))<0){
		perror("recv");
		exit(-1);
	}
	if (nbytes > 0){
	if(msg[0]=='H' )
	{
	
		char  path1[256]="";
		strcat(path1,G_INCOMINGPATH);
		strcat(path1,"/");
		strcat(path1,archivo_recibir);
		block_recv(socket, (char *) path1, msg);
	}
	if((msg[0]=='G') &&  (msg[1]=='E')  && (msg[2]=='T'))
	{
		char nombre[256];
		char path[256]="";
		int k =0;
		char * puntero =strstr(msg,"HTTP/1.1\r\n");
		char * puntero1 =(char *) msg +3;
		int len = puntero -puntero1;
		for(k=0;k<256;k++)
			nombre[k]=(char)0x00;
		k=0;
		while(k<len-1)
		{
			nombre[k]=msg[k+4];
			k++;
		}
		strcat(path,G_SHAREPATH);
		strcat(path,"/");
		strcat(path,nombre);
		block_sending(socket,path);
		
	}
	if(msg[16]==-128){
		char * criterio;
		char payload[4];
		int longitud;	
		char * query_hit;
		int numberOfhits;
		for(i=0;i<4;i++)
		{
				payload[i]=msg[22-i];
		}
		longitud=atoi(payload);
		longitud= longitud - 3;
		criterio=(char *)malloc(longitud);
			
		for(i=0;i<longitud;i++)
			criterio[i]=msg[25+i];
		numberOfhits= get_archivos(criterio);
		longitud = crear_query_hit(&query_hit,numberOfhits);	
		if(send(socket, (char *)query_hit, longitud, 0)<0)
		{
			perror("send");
		}
				
		
	}
	if(msg[16]==-127)
	{
		int num_querys;	
		num_querys=decodificar_query_hit(msg,socket);
		query_hits_recibidas++;
		if(query_hits_recibidas==get_total_conexiones())
		{
			for(i=0;i<numero_query_hit;i++)
				printf("%d.)nombre: %s\n tamaño %u\n ip_puerto %s\n",i+1,nombres_hit[i],tamanios_hit[i],ip_puerto_hit[i]);
			query_hits_recibidas=0;
	
		}
	free(msg);
	}
}else{

	int x;
	for(x= 0; x < MAX_CONEXIONES; x++){
		if(socket_con[x] == socket)
			socket_con[x] = -1;
	}
	if (close(socket)<0){
		perror("close");
						
	}
	break;
}
}
}
printf("hilo recibir termina\n");
	/*if(G_RECIBIR == 1){
	if (close(socket)<0){
		perror("close");
						
	}
	}*/
	return 0;
}

char* get_ip(){
	char Buf [ 200 ] ;	
	struct hostent* Host;
	gethostname ( Buf , 200 ) ;
	Host = ( struct hostent * ) gethostbyname ( Buf );	
return inet_ntoa(*((struct in_addr *)Host->h_addr));
}

int short2leb(char *dest, unsigned short v) {
       *dest++ = (v      ) & 0x000000FF;
       *dest++ = (v >>  8) & 0x000000FF;
            return 2;
}unsigned short leb2short(char *orig) {
       			return (unsigned short) (((unsigned char)orig[1] << 8) |(unsigned char)orig[0]);
}

unsigned int int2leb(char *dest, int v) {
       *dest++ = (v      ) & 0x000000FF;
       *dest++ = (v >>  8) & 0x000000FF;
       *dest++ = (v >> 16) & 0x000000FF;
       *dest++ = (v >> 24) & 0x000000FF;
       return 4;
}


unsigned int leb2int(char *orig) {
       return (unsigned int) (((unsigned char)orig[3] << 24) |
                             ((unsigned char)orig[2] << 16) |
                             ((unsigned char)orig[1] << 8) |
                              (unsigned char)orig[0]);
}
int block_sending(int sock,char *path) 
{ 
	int  ofs, nbytes = 0, block = BLOCK;
	unsigned int size=0;
	char pbuf[BLOCK]; 
	char * cadena_size;
	char  msg[MAX_MESSAGE]="";
	char respuesta[5]; 
	struct stat  archivo;
	FILE *f;	
		
    	cadena_size=(char*)malloc(20);
   	f =fopen(path,"r");
	stat(path, &archivo);
	size=archivo.st_size;
	sprintf(cadena_size,"%d",size);
  	strcat(msg,"HTTP/1.1 200 OK\r\nServer: ROtella\r\nContent-type: application/binary\r\nContent-length: ");
	strcat(msg,(char*)cadena_size);
	strcat(msg,"\r\n\r\n");
	if(send(sock, msg, MAX_MESSAGE, 0)<0){
		perror("send");
		exit(-1);
	}

	for(ofs = 0; block == BLOCK; ofs += BLOCK) 
	{ 
		if(size - ofs < BLOCK) 
      		block = size - ofs; 
      		
		fread(pbuf,sizeof(char),block,f);
		
		do 
		{ 
		nbytes += send(sock, pbuf, block, 0);
		recv(sock, respuesta, 2, 0 );
		respuesta[2] = '\0';
		}while( strcmp(respuesta, "KO") == 0); 
	} 
	fclose(f);
return nbytes; 
} 

int block_recv(int sock, char * path,char * msg)
{
	int size,ofs,nbytes=0,bytesTotales=0,block=BLOCK;
	char pbuf[BLOCK];
	char * size_cadena;
	int correcto =1;
	char * puntero;
	char * puntero1;
	int len=0;
	int k=0;
	FILE *f;
	printf("Recibiendo archivo\n");
	size_cadena= (char*) malloc(20);
	
	
	f =fopen(path,"w");
	if(f==NULL)
	{
		mkdir(G_INCOMINGPATH,0755);
		f =fopen(path,"w");
	}
	puntero = strstr(msg,"Content-length: ")+16;
	puntero1= strstr(msg,"\r\n\r\n");
	len= puntero1-puntero;
	while(k<len)
	{
		*(size_cadena + k)=*(puntero +k);
		k++;	
	}
	size=atoi(size_cadena);	
	for(ofs=0;block==BLOCK;ofs+=BLOCK)
	{
		if(size-ofs<BLOCK)
			block = size -ofs;
		correcto=1;
		do
		{
			nbytes=recv(sock,pbuf,block,0);	
			if(nbytes==block)
			{
				send(sock,"OK",2,0);
				fwrite(pbuf,block,1,f);
				bytesTotales+=nbytes;
			}
			else
			{
			send(sock,"KO",2,0);
			correcto=0;
	
			}
		}
		while (correcto==0);
	}
	fclose(f);
	/*copiamos el archivo a downloads y lo borramos*/
	mover_a_downloads(path);
	printf("Archivo: %s Recibido\n",archivo_recibir);
return nbytes;
}
void mover_a_downloads(char * path)
{
	char  path_output[256]="";
	struct stat  archivo;
	int  ofs, block = BLOCK;
	unsigned int size=0;
	char pbuf[BLOCK];
	FILE *input,*output;
	stat(path, &archivo);
	size=archivo.st_size;
	input =fopen(path,"w");
	if(input==NULL)
	{
		mkdir(G_INCOMINGPATH,0755);
		input =fopen(path,"w");
	}
	strcat(path_output,G_DOWNLOADPATH);
	strcat(path_output,"/");
	strcat(path_output,archivo_recibir);
	output =fopen(path_output,"w");
	if(output==NULL)
	{
		mkdir(G_DOWNLOADPATH,0755);
		output =fopen(path_output,"w");
	}
	for(ofs = 0; block == BLOCK; ofs += BLOCK) 
	{ 
		if(size - ofs < BLOCK) 
      		block = size - ofs; 
		fread(pbuf,sizeof(char),block,input);
		fwrite(pbuf,block,1,output);
	} 
	fclose(input);
	fclose(output);
	remove(path);

}



