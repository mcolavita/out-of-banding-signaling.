 
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
#include <sys/select.h>
#define _POSIX_C_SOURCE 200809L
#define MAX_MESSAGE 50

#define UNIX_PATH_MAX 108


/*Struttura utilizzata per calcolare 
  il tempo di arrivo dei messaggi*/
struct timespec tempo;

//Struttura per non bloccarmi sulla select
struct timeval tm;


//Struttura per i messaggi da server a supervisor
typedef struct pipe_mex{
    uint64_t nid;
    int s_est;
    int s;
}pipe_mex;


//Struttura per costruire la tabella finale
typedef struct sup_mex{
    uint64_t c_id;
    int sup_esitmate;
    int n_servers;
}sup_mex;

/*Variabili che uso per controllare il numero di SIGINT*/
volatile sig_atomic_t sig_counter,termina,interrupted;

/*Numero di server che saranno lanciati*/
int k;

int **all_pipe; //Array che utilizzo per le pipe
int *all_server; //Array che utilizzo per i server

sup_mex* all_est; //tabella che utilizzo per il segnale di SIGINT 

/*Dimensione della tabella*/
int n;

/*Funzione che mi converte l'id in network byte order in host byte order*/
uint64_t htonll(uint64_t value){
    int num = 42;
    if (*(const char*)(&num) == num){
        uint32_t high_part = htonl((uint32_t)(value >> 32));
        uint32_t low_part = htonl((uint32_t)(value & 0xFFFFFFFFLL));
        return ((uint64_t)(low_part) << 32) | high_part;
    }
     else 
        return value;
}

/*Funzione che fa la stima del contuento dell'array con i tempi di ogni messaggio.
  La funzione viene eseguita da ogni server scelto dal client*/
int stima(int t, long long int *w_time){
    int estimated;
    /*Controllo che eseguo al fine di restituire come stima 0 
     se ci sono  0 o 1 elementi all'interno dell'array*/
    if(t==0 || t==1){
        estimated=0;
        return estimated;
    }
    int i,tmp;
    int min = 1000000000;
    for(i=1;i<t;i++){
        tmp = w_time[i]- w_time[i-1];
        if(abs(tmp) < min )
            min = tmp;
    }
    return min;                  
}


/*Il thread worker leggerà dalla socket fino a quando ci saranno
  dei messaggi ed infine invierà al supervisor un messaggio di tipo pipe_mex*/
static void *worker(void *arg){
    int *who_is = (int*) arg;
    int fd_s,i;
    fd_s=who_is[0];
    i=who_is[1];
    int conn;
    int t=0;
    uint64_t n_id=0;
    uint64_t id=0;
    //Definisco e alloco array di tempi di ricezione messaggi
    unsigned long long int* w_time;
    w_time=(long long int*)malloc(MAX_MESSAGE*sizeof(long long int));
    //Ciclo fino a quando non ricevo più messaggi dal client
    while((conn=read(fd_s,&n_id,sizeof(uint64_t)))>0){
        clock_gettime(CLOCK_REALTIME, &tempo);
        id=htonll(n_id);
        w_time[t] = (((unsigned long long int)tempo.tv_sec % 10000) * 1000) + ((unsigned long long int)tempo.tv_nsec / 1000000);
        fflush(stdout);
        printf("SERVER %d INCOMING FROM %016lX @ %lld\n",(i+1),id,w_time[t]);
        fflush(stdout);
        t++;            
    }
    //Chiusura della socket
    close(fd_s);
    int est=stima(t,w_time);
    if(est >0  && id!=0){ 
        /*Setto il messaggio da inviare 
          al supervisor con id e stima del client e server che ha prestato servizio */
        pipe_mex s_mex;
        s_mex.nid = id;
        s_mex.s_est = est;
        s_mex.s = i;
        fflush(stdout);
        //SInvio del messaggio al supervisor
        if((write(all_pipe[i][1],&s_mex,sizeof(pipe_mex)))==-1){
            perror("Errore nella pipe\n");
            exit(EXIT_FAILURE);
        }
        fflush(stdout);
        printf("SERVER %d CLOSING %016lX ESTIMATE %d\n",(i+1),id,est);
        fflush(stdout);
    }
    
    pthread_exit(NULL);
}

/*Funzione che mi accetta le connessioni chiamando la funzione Worker*/
static void *connection(void *arg){
    int *who_is = (int*)arg;
    int fd_s,i;
    fd_s=who_is[0];
    i=who_is[1];
    pthread_t th;
    while(1){
        int conn;
        if((conn=accept(fd_s,NULL,NULL))==-1){
            perror("Errore nella connection ");
            exit(EXIT_FAILURE);
        }
        //Se è avvenuta la connessione avvio il thread worker
        if(conn>0){
            printf("SERVER %d CONNECT FROM CLIENT\n",(i+1));
            fflush(stdout);
            who_is[0]=conn; //socket 
            who_is[1]=i; //server
            if((pthread_create(&th,NULL,&worker,who_is))!=0){
                perror("Errore nella creazione del thread");
                exit(EXIT_FAILURE);
            }
        }
    }
    pthread_exit(NULL);
    
}


/*Funzione che crea le socket per ogni server lanciato, ogni server sarà un thread*/
void create_server(int i,int *all_server){
    if((all_server[i]=fork())==-1){
        perror("Errore nella fork\n");
        exit(EXIT_FAILURE);
    }
    //codice dedicato ai server
    if(all_server[i]==0){ //Mi trovo nel figlio
        printf("SERVER %d ACTIVE\n",(i+1));
        fflush(stdout);
        //Definisco le variabili che utilizzerò
        struct sockaddr_un sa;
        int fd_s;
        pthread_t th;
        char name_s[15]="OOB-server-";
        sprintf(name_s+11,"%d",(i+1));
        name_s[strlen(name_s)+1]='\0';
        strncpy(sa.sun_path,name_s,UNIX_PATH_MAX);
        sa.sun_family=AF_UNIX;

        //creazione della socket
        if((fd_s=socket(AF_UNIX,SOCK_STREAM,0))==-1){
            perror("Errore nella socket\n");
            exit(EXIT_FAILURE);
        }
        if((bind(fd_s,(struct sockaddr *)&sa,sizeof(sa)))==-1){
            perror("Errore nella bind\n");
            exit(EXIT_FAILURE);
        }

        if((listen(fd_s,SOMAXCONN))==-1){
            perror("Errore nella listen\n");
            exit(EXIT_FAILURE);
        }

        /*creo un array di due elementi che manderò al dispatcher
            per dire quale socket e quale server usare*/
        int *who_is=(int*)malloc(2*sizeof(int));
        who_is[0]= fd_s; //socket
        who_is[1]= i; //server

        //maschero i segnali destinati al supervisor
        sigset_t _set;
        sigfillset(&_set);
        pthread_sigmask(SIG_SETMASK,&_set,NULL);

        if((pthread_create(&th,NULL,&connection,who_is))!=0){
            perror("Errore nella creazione del thread\n");
            exit(EXIT_FAILURE);
        }
        if((pthread_join(th,NULL))!=0){
            perror("Errore join thread\n");
            exit(EXIT_FAILURE);
        }
        exit(0);        
    }       
}
/*Funzione che servirà per creare la tabella che
  verrà stampata su stdout e stderr. Aggiunge i vari secret calcolati dai server
  e se ne trova uno migliore aggiorna la tabella per quel client.*/
void table(pipe_mex rec){
    int i=0;
    int estimate = rec.s_est;
    uint64_t id = rec.nid;
    int trovato = 0;
    //ciclo fino a quando o trovo un stesso client
    while(i<n && !trovato){
        if( id == all_est[i].c_id){
            if(estimate < all_est[i].sup_esitmate){
                all_est[i].sup_esitmate=estimate; //aggiorno con la migliore stima 
            }
            (all_est[i].n_servers)++; // server che hanno prestato servizio per quel client
            trovato=1;
        }
        else i++;
    }
    //se non trovo il client nella tabella, lo aggiungo e incremento la dimensione della tabella
    if(!trovato){
        all_est[n].c_id=id;
        all_est[n].sup_esitmate=estimate;
        all_est[n].n_servers=1;
        n++;
    }
}


/*Termina tutti i processi che sono rimasti in esecuzione
  dopo la chiusura del supervisor */
void all_kill(int *all_server){
    int i;
    for(i=0; i<k;i++){
        kill(all_server[i],9);
    }
    free(all_server);
}


/*Gestore segnale sigalarm*/
static void sigalrm_handler (int signum) {
	sig_counter=0;
    interrupted=0;
    termina=0;
}



/*Gestore segnale SIGINT*/
static void sigint_handler (int signum) {
	if (!sig_counter) {
		sig_counter = 1;
		alarm(1);
        interrupted = 1;
    }
	else {
		termina =1;
	}
    
}
/*Funzione che entra in un ciclo infinito per leggere dalle pipe
  tutti i messaggi che sono stati inviati dai servers al supervisor*/
void s_read(){
    pipe_mex rec; //messaggio di tipo SERVER-SUPERVISOR
    fd_set set, rdset;
    int i;
    /*Preparazione alla select*/
    FD_ZERO(&set);
    for (i = 0; i < k ; i++){
        FD_SET(all_pipe[i][0], &set);
    }
    /*Ciclo infinito dove vado a leggere di continuo i messaggi inviati dai server*/
    while(!termina){
        if(interrupted==1){
            for(i=0;i<n;i++){
            fprintf(stderr,"SUPERVISOR ESTIMATE %d FOR %016lX BASED ON %d\n",all_est[i].sup_esitmate,all_est[i].c_id,all_est[i].n_servers);
            }
            interrupted=0;
        }
        //timer per la select
        tm.tv_sec = 0;
		tm.tv_usec = 3000;
        rdset = set;
        if (select(FD_SETSIZE, &rdset, NULL, NULL,&tm) < 0){
            if (errno != EINTR) {
                perror("Errore nella select\n");
                exit (EXIT_FAILURE);
            }
        }
        else {
            for (int fd = 0; fd < FD_SETSIZE; fd++){
                if (FD_ISSET(fd, &rdset)){
                    int valread=0;
                    if ((valread = read(fd, &rec , sizeof(pipe_mex) ) == -1)){
                        exit(EXIT_FAILURE);
                    }
                    fflush(stdout);
					printf("SUPERVISOR ESTIMATE %d FOR %016lX FROM %d\n", rec.s_est, rec.nid,(rec.s)+1);
                    fflush(stdout);
                    table(rec);
                    
                }
            }
        }
    }
    fprintf(stdout,"\n");
    fflush(stdout);
    //Stampo ogni riga della tabella costruita prima di uscire    
    for(int i=0;i<n;i++){
        fprintf(stdout,"SUPERVISOR ESTIMATE %d FOR %016lX BASED ON %d\n",all_est[i].sup_esitmate,all_est[i].c_id,all_est[i].n_servers);
        fflush(stdout);
        
    }
	fprintf(stdout,"SUPERVISOR EXITING\n");
    fflush(stdout);
    //Tutti i processi attivi con la funzione kill
    all_kill(all_server);
 }

int main(int argc, char *argv[]){
    if(argc!=2){
        printf("Mandare al Supervisor il numero di Server da lanciare!\n");
        return -1;
    }

    int i;
    k= atoi(argv[1]);
    printf("SUPERVISOR STARTING %d\n",k);
    fflush(stdout);
    all_pipe=(int **)malloc(k*sizeof(int*)); 
    //Creazione delle pipe per la comunicazione
    for(i=0;i<k;i++){
        all_pipe[i]=(int *)malloc(2*sizeof(int));
        if((pipe(all_pipe[i]))==-1){
            perror("Errore nella pipe\n");
            exit(EXIT_FAILURE);
        }  
    }
    //Creazione dei k server. 
    all_server=(int*)malloc(k*sizeof(int)); //allocazione array di server 
    for(i=0; i<k;i++){
        create_server(i,all_server); //Esecuzione dei k server
    }
    int x;
    //Definisco i segnali che vengono utilizzati
    struct sigaction s, salrm;
    memset(&s,0,sizeof(s));
    memset(&salrm,0,sizeof(salrm));
    //Gestore SIGINT
    s.sa_handler=sigint_handler;
    s.sa_flags = SA_RESTART;
    //Gestore SIGALRM
    salrm.sa_handler=sigalrm_handler;
    salrm.sa_flags = SA_RESTART;
    sigaction(SIGINT,&s,NULL);
    sigaction(SIGALRM,&salrm,NULL);
    sig_counter=0;
   
    all_est=malloc(100*sizeof(sup_mex)); //Alloco un massimo di 100 per la tabella che stamperò
    n=0; //Inizializzo la variabile globale che userò per scorrere la tabella
    s_read();
    fflush(stdout);
    return 0;
}

