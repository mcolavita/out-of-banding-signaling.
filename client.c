#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#define UNIX_PATH_MAX 108

struct timespec tempo;


/*Funzione che utilizzo per trovare il primo server
  libero tra i k sever disponibili*/
int distinti (int i, int* server) {
	int j=0;
	int trovato = 1;
	
	while (j<i && trovato) {
		if (server[j] == server[i])
			trovato = 0;
		j++;
	}
	return trovato;
}

/*Genera in maniera randomica un intero di 64 bit facendo ausilio della funzione getpid*/
uint64_t rand_uint64(void) {
  srand(getpid());
  int a = rand();
  int b = rand();
  int c = rand();
  int d = rand();
  long e = (long)a*b;
  e = abs(e);
  long f = (long)c*d;
  f = abs(f);
  long long answer = (long long)e*f;
  return answer;
}


/*Genera un numero in network byte order con l'intero a 64 bit
calcolato con la funzione rand_unit64 */
uint64_t htonll(uint64_t value)
{
    int num = 42;    
    if (*(const char*)(&num) == num)
    {
        uint32_t high_part = htonl((uint32_t)(value >> 32));
        uint32_t low_part = htonl((uint32_t)(value & 0xFFFFFFFFLL));

        return ((uint64_t)(low_part) << 32) | high_part;
    } else
    {
        return value;
    }
}

/*Funzione che utilizzo per riempire un array di server
  creare e connettermi alle socket */
void connection(int k, int p, int *all_fd){
    int i,j;
    int* w_server=(int*)malloc(p*sizeof(int));
    struct sockaddr_un* sa;
    sa=malloc(p*sizeof(struct sockaddr_un));
    char name_s[15]="OOB-server-";
    //costruisco l'array di server con solo indici
    for(i=0;i<p;i++){
        w_server[i]=(rand() %k) +1;
    
        while(!distinti(i,w_server))
            w_server[i]=(rand() %k) +1;

        sprintf(name_s+11,"%d",w_server[i]);
        name_s[strlen(name_s)+1]='\0';
        strncpy(sa[i].sun_path,name_s,UNIX_PATH_MAX);
		sa[i].sun_family=AF_UNIX;
        
        if((all_fd[i]= socket(AF_UNIX,SOCK_STREAM,0))==-1){
            perror("Errore nella socket");
            exit(EXIT_FAILURE);
        }

        if((connect(all_fd[i],(struct sockaddr*)&(sa[i]),sizeof(sa[i])))==-1){
            perror("Errore nella connect");
            exit(EXIT_FAILURE);
        }
    }
}

//Funzione che chiude tutte le socket lato client
void all_close(int *all_fd,int p){
    int i;
    for(i=0;i<p;i++){
        if((close(all_fd[i]))==-1){
            perror("Errore nella close");
            exit(EXIT_FAILURE);
        }
    }   
}

int main(int argc, char *argv[]){
    if(argc!=4){
        printf("Inserire i parametri corretti");
        return -1;
    }
    uint64_t id, id_n;
    int p,k,w,secret;
    p=atoi(argv[1]);
    k=atoi(argv[2]);
    w=atoi(argv[3]);

    if(p < 1 || p > k || w<=3*p){
        printf("Non sono state rispettate le proprietà dei parametri\n");
        return -1;
    }

    srand(getpid()); //utilizzo il pid come seme
    secret=(rand() % 3000) +1;
    id=rand_uint64();
    id_n=htonll(id);
    int* all_fd=(int*)malloc(p*sizeof(int));
    printf("CLIENT %016lX SECRET %d\n",id,secret);
    fflush(stdout);
    connection(k,p,all_fd);
    int secondi = secret / 1000;
    tempo.tv_nsec = (secret % 1000) * 1000000;
    tempo.tv_sec = secondi;
    int i;
    /*itero per i w messaggi che andrò ad inviare */
   for(i=0;i<w;i++){   
        int w_server=rand()%p;
        if((write(all_fd[w_server],&id_n,sizeof(uint64_t)))==-1){
            perror("Errore nella write");
            exit(EXIT_FAILURE);
        }
        nanosleep(&tempo,NULL);
    }
    all_close(all_fd,p);
    printf("CLIENT %016lX DONE\n",id);
    fflush(stdout);
    return 0;

}
