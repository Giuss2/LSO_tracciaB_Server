#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include "mappa.h"
#include <time.h>

#define PORT    5201
#define BACKLOG 8
#define NUM_PLAYERS 12
pthread_mutex_t mtx;
Mappa mappaGlobale;
Player* players[NUM_PLAYERS];

typedef struct messServer{
     Mappa mappaPlayer;
     Player p;
     Player players[NUM_PLAYERS];
}MessServer;

typedef struct messClient{
    char direzione;
    bool movimento;
}MessClient;

void invioMappaLocale(Player *p, Mappa *mappaLocale, Mappa *mappa, char direzione);
void addPlayer(Player* p){
    pthread_mutex_lock(&mtx);
    for(int i = 0; i < NUM_PLAYERS; i++){
        if(players[i] == NULL){
            players[i] = p;
            break;
        }
    }
    pthread_mutex_unlock(&mtx);
}

static void *handle_client(void *arg) {
    int fd = *(int *)arg;
    free(arg);
    char buf[N*N];
    Mappa mappaLocale;

    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned long)pthread_self();

    // Inizializza tutta la mappa locale con caratteri spazio ' '
    memset(mappaLocale.mappa, ' ', sizeof(mappaLocale.mappa));
    memset(mappaLocale.mappaPlayer, ' ', sizeof(mappaLocale.mappaPlayer));

    int num_simboli = sizeof(simboli) / sizeof(simboli[0]);
    char lettera_random = simboli[rand_r(&seed) % num_simboli];
    Colore colore_random = (Colore)((rand_r(&seed) % 12) + 4);
    printf("Lettera random: %c, Colore random: %d\n", lettera_random, colore_random);

    Player p = {
        lettera_random,
        5,
        7,
        colore_random
    };

    addPlayer(&p);

    int posizione_riga;
    int posizione_colonna;

    pthread_mutex_lock(&mtx);
    do {

        posizione_riga = rand_r(&seed) % N;
        posizione_colonna = rand_r(&seed) % N;

        if(mappaGlobale.mappa[posizione_riga][posizione_colonna] == cella_libera) {
            p.colonna = posizione_colonna;
            p.riga = posizione_riga;
        }

    } while(mappaGlobale.mappa[posizione_riga][posizione_colonna] != cella_libera);

    mappaGlobale.mappa[p.riga][p.colonna] = p.lettera;
    mappaLocale.mappa[p.riga][p.colonna] = p.lettera;

    mappaGlobale.mappaPlayer[p.riga][p.colonna] = p.lettera;
    mappaLocale.mappaPlayer[p.riga][p.colonna] = p.lettera;

    pthread_mutex_unlock(&mtx);

    MessClient messClient;
     for (;;) {
        ssize_t n = recv(fd, &messClient, sizeof(messClient), 0);
        if (n == 0) break;
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("recv");
            break;
        }
    printf("Ricevuto: %c, movimento: %d\n", messClient.direzione, messClient.movimento);
    
    
// SE il client richiede il movimento, aggiorna le coordinate, altrimenti rivela solo la nebbia iniziale
    if (messClient.movimento) {
        invioMappaLocale(&p, &mappaLocale, &mappaGlobale, messClient.direzione);
    } else {
        pthread_mutex_lock(&mtx);
    // Solo per la prima mappa statica
        rivelaNebbia(&p, &mappaLocale, &mappaGlobale);
        pthread_mutex_unlock(&mtx);
    }
    

    MessServer messServer;
    messServer.p = p;
    messServer.mappaPlayer = mappaLocale;  
    
    for(int i = 0; i < NUM_PLAYERS; i++){
        if(players[i] != NULL){
            messServer.players[i] = *players[i];
        }else
            messServer.players[i].lettera = '\0'; 
    }

// Invia i dati aggiornati (o iniziali) al client
        ssize_t off = 0;

        while (off < sizeof(messServer)) {
            ssize_t w = send(fd, &messServer, sizeof(messServer), MSG_NOSIGNAL);
            if (w < 0) {
                if (errno == EINTR) continue;
                perror("send");
                close(fd);
                return NULL;
            }
           off += w;
        }
    }
    

    close(fd);
    return NULL;
}

int main(void) {

    pthread_mutexattr_t attr;
    Player* p = NULL;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    pthread_mutex_init(&mtx, &attr);


    signal(SIGPIPE, SIG_IGN);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(s, BACKLOG) < 0) { perror("listen"); return 1; }

    printf("Server in ascolto su 0.0.0.0:%d (thread per connessione)\n", PORT);

    int num_simboli = sizeof(simboli) / sizeof(simboli[0]);

    unsigned int seed = time(NULL) ^ pthread_self();

    rand_r(&seed);

    for(int i = 0; i < N; i++) {
        for(int j = 0; j < N; j++) {

            if((rand_r(&seed) % 100) > 20)
                mappaGlobale.mappa[i][j] = simboli[0];
            else
                mappaGlobale.mappa[i][j] = simboli[1];

            //mappaLocale.mappaPlayer[i][j] = '0';
        }
    }

    for(int i=0; i < NUM_PLAYERS; i++){
        players[i] = NULL;
    }

    for (;;) {
        int c = accept(s, NULL, NULL);
        if (c < 0) { if (errno == EINTR) continue; perror("accept"); continue; }

        int *fdp = malloc(sizeof(int));
        if (!fdp) {
            perror("malloc");
            close(c);
            continue;
        }
        *fdp = c;

         printf("Connesso");

        pthread_t tid;
        int rc = pthread_create(&tid, NULL, handle_client, fdp);
        if (rc != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(rc));
            close(c);
            free(fdp);
            continue;
        }
        pthread_detach(tid);

	
    }

    pthread_mutexattr_destroy(&attr);
}


void rivelaNebbia(Player *p, Mappa* mappa, Mappa* mappaGlobale){

    for(int i = p->riga - 1; i <= p->riga + 1; i++) {

        for(int j = p->colonna - 1; j <= p->colonna + 1; j++) {

            if(j < 0 || i < 0 || j > N - 1 || i > N - 1)
                continue;

            if(mappa->mappa[i][j] == ' '){
                mappa->mappa[i][j] = mappaGlobale->mappa[i][j];
                mappa->mappaPlayer[i][j] = mappaGlobale->mappaPlayer[i][j];
            }
                

        }
    }
   
}


bool verificaMossa(int riga, int colonna, char mappa[N][N]) {

    if(colonna < 0 || riga < 0 ||
       colonna > N - 1 || riga > N - 1)
        return false;

    if(mappa[riga][colonna] == MURO)
        return false;
    
    if(mappa[riga][colonna] != cella_libera)
        return false;

    return true;
}

void invioMappaLocale(Player *p, Mappa *mappaLocale, Mappa *mappaGlobale, char direzione) {
    pthread_mutex_lock(&mtx);

    int riga_nuova = p->riga;
    int colonna_nuova = p->colonna;

    switch(direzione) {
        // --- SPOSTAMENTI ORIZZONTALI (Colonne) ---
        case 'A':
        case 'a':
            colonna_nuova--; 
            break;

        case 'D':
        case 'd':
            colonna_nuova++; 
            break;

        // --- SPOSTAMENTI VERTICALI (Righe) ---
        case 'W':
        case 'w':
            riga_nuova--;   
            break;

        case 'S':
        case 's':
            riga_nuova++; 
            break;
    }

    rivelaNebbia(p, mappaLocale, mappaGlobale);

    if(verificaMossa(riga_nuova, colonna_nuova, mappaGlobale->mappa)) {
        mappaLocale->mappa[p->riga][p->colonna] = cella_libera;
        mappaGlobale->mappa[p->riga][p->colonna] = cella_libera;

        p->colonna = colonna_nuova;
        p->riga = riga_nuova;

        mappaLocale->mappaPlayer[p->riga][p->colonna] = p->lettera;
        mappaGlobale->mappaPlayer[p->riga][p->colonna] = p->lettera;

        mappaLocale->mappa[p->riga][p->colonna] = p->lettera;
        mappaGlobale->mappa[p->riga][p->colonna] = p->lettera;

        rivelaNebbia(p, mappaLocale, mappaGlobale);
    }

    pthread_mutex_unlock(&mtx);
}
