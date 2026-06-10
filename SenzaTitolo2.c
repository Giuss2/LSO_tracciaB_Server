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
#include <stdatomic.h>

#define PORT    5201
#define BACKLOG 8
#define NUM_PLAYERS 12

pthread_mutex_t mtx;
atomic_bool game_over = false;
struct timespec end_time;


typedef struct statistiche {
    char id;
    int celleConquistate;
} Statistiche;

Mappa mappaGlobale;
Player* players[NUM_PLAYERS];

int player_fds[NUM_PLAYERS];

typedef struct messServer{
     Mappa mappaPlayer;
     Player p;
     Player players[NUM_PLAYERS];
     MsgType type;
     Statistiche statistics[NUM_PLAYERS];
}MessServer;

typedef struct messClient{
    char direzione;
    bool movimento;
}MessClient;

int check_game_over();
static ssize_t writen_all(int fd, MessServer *mess);
static ssize_t readn_all(int fd, void *buf, size_t len);
void invioMappaLocale(Player *p, Mappa *mappaLocale, Mappa *mappa, char direzione);
void addPlayer(Player* p);
void broadcast_game_over();
void calcoloStatistiche(Statistiche* stats, Mappa* mappaGlobale);

void *timer_thread(void *arg) {
    (void)arg;
    int secondi_passati = 0;
    int durata_partita = 60; 
    int T = 10;               // Invia la mappa globale ogni tot sec
    Statistiche vincitore = { .id = '\0', .celleConquistate = 0 };

    while (secondi_passati < durata_partita) {
        sleep(1);
        secondi_passati++;

        // Ogni T secondi invia la mappa globale a tutti (Richiesta della specifica)
        if (secondi_passati % T == 0 && !game_over) {
            pthread_mutex_lock(&mtx);
            
            MessServer msg_periodico;
            memset(&msg_periodico, 0, sizeof(msg_periodico));
            msg_periodico.type = MSG_GLOBAL_UPDATE;

            Statistiche statistics[NUM_PLAYERS];
            memset(statistics, 0, sizeof(statistics));
            for (int i = 0; i < NUM_PLAYERS; i++) {
                Statistiche stat;
                stat.id = players[i] ? players[i]->lettera : '\0';
                stat.celleConquistate = 0; // Inizializza a 0
                
                calcoloStatistiche(&stat, &mappaGlobale);
                statistics[i] = stat;
            }

            for(int i = 0; i < NUM_PLAYERS; i++){
                msg_periodico.statistics[i] = statistics[i];
                if (!players[i]) {statistics[i].id = '\0'; statistics[i].celleConquistate = 0; continue;}

                if (msg_periodico.statistics[i].celleConquistate > vincitore.celleConquistate) {
                        vincitore.id = msg_periodico.statistics[i].id;
                        vincitore.celleConquistate = msg_periodico.statistics[i].celleConquistate;
                    }
            }

            //nascondi muri
            for (int i = 0; i < N; i++) {
                for (int j = 0; j < N; j++) {
                    if(mappaGlobale.mappa[i][j] == MURO)
                        msg_periodico.mappaPlayer.mappa[i][j] = '.';
                    else{
                        msg_periodico.mappaPlayer.mappa[i][j] = mappaGlobale.mappa[i][j];
                    }                    
                    msg_periodico.mappaPlayer.mappaPlayer[i][j] = mappaGlobale.mappaPlayer[i][j];
                }
            }

            
            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (players[i]) msg_periodico.players[i] = *players[i];
            }

            for (int i = 0; i < NUM_PLAYERS; i++) {
                if (player_fds[i] != -1) {
                    // Personalizziamo il player corrente per ciascun client
                    if (players[i]) msg_periodico.p = *players[i];
                    send(player_fds[i], &msg_periodico, sizeof(msg_periodico), MSG_NOSIGNAL);
                }
            }
            pthread_mutex_unlock(&mtx);
        }
    }

    // Tempo scaduto!
    atomic_store(&game_over, true);
    printf("Tempo scaduto! Invio GAME OVER a tutti E VINCITORE %c...\n", vincitore.id);
    broadcast_game_over(&vincitore);

    _exit(0);
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
    
    Player *p = malloc(sizeof(Player));
    *p = (Player) {
        lettera_random,
        5,
        7,
        colore_random
    };


    int posizione_riga;
    int posizione_colonna;

    pthread_mutex_lock(&mtx);
    addPlayer(p);
    do {

        posizione_riga = rand_r(&seed) % N;
        posizione_colonna = rand_r(&seed) % N;

        if(mappaGlobale.mappa[posizione_riga][posizione_colonna] == cella_libera) {
            p->colonna = posizione_colonna;
            p->riga = posizione_riga;
        }

    } while(mappaGlobale.mappa[posizione_riga][posizione_colonna] != cella_libera);

    mappaGlobale.mappa[p->riga][p->colonna] = p->lettera;
    mappaLocale.mappa[p->riga][p->colonna] = p->lettera;

    mappaGlobale.mappaPlayer[p->riga][p->colonna] = p->lettera;
    mappaLocale.mappaPlayer[p->riga][p->colonna] = p->lettera;

    pthread_mutex_unlock(&mtx);

    MessClient messClient;
    while (!game_over) {
          if (atomic_load(&game_over))
        break;

    ssize_t n = readn_all(fd, &messClient, sizeof(messClient));
    if (n == 0)
        break;

    if (n < 0) {
        perror("recv");
        break;
    }

    if (messClient.movimento) {
        invioMappaLocale(p, &mappaLocale, &mappaGlobale, messClient.direzione);
    } else {
        pthread_mutex_lock(&mtx);
        rivelaNebbia(p, &mappaLocale, &mappaGlobale);
        pthread_mutex_unlock(&mtx);
    }

    MessServer messServer;
    messServer.p = *p;
    messServer.mappaPlayer = mappaLocale;
    messServer.type = MSG_UPDATE;

    pthread_mutex_lock(&mtx);
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (players[i])
            messServer.players[i] = *players[i];
        else
            messServer.players[i].lettera = '\0';
    }
    pthread_mutex_unlock(&mtx);


    if (writen_all(fd, &messServer)<0) {
        perror("send");
        break;
    }
}

pthread_mutex_lock(&mtx);

for (int i = 0; i < NUM_PLAYERS; i++) {
    if (player_fds[i] == fd) {
        player_fds[i] = -1;
        players[i] = NULL;
        break;
    }
}

pthread_mutex_unlock(&mtx);
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

    int opt = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        close(s);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(s, BACKLOG) < 0) { perror("listen"); return 1; }

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    end_time.tv_sec += 30000; // durata partita

    pthread_t timer;
    pthread_create(&timer, NULL, timer_thread, NULL);
    pthread_detach(timer);

  


    printf("Server in ascolto su 0.0.0.0:%d (thread per connessione)\n", PORT);

    int num_simboli = sizeof(simboli) / sizeof(simboli[0]);

    unsigned int seed = time(NULL) ^ pthread_self();

    rand_r(&seed);

    pthread_mutex_lock(&mtx);
    for(int i = 0; i < N; i++) {
        for(int j = 0; j < N; j++) {
            if((rand_r(&seed) % 100) > 20)
                mappaGlobale.mappa[i][j] = simboli[0];
            else
                mappaGlobale.mappa[i][j] = simboli[1];
        }
    }

    for(int i=0; i < NUM_PLAYERS; i++){
        players[i] = NULL;
        player_fds[i] = -1;
    }
    pthread_mutex_unlock(&mtx);

    for(;;) {
        int c = accept(s, NULL, NULL);
        if (c < 0) { if (errno == EINTR) continue; perror("accept"); continue; }

    pthread_mutex_lock(&mtx);
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (player_fds[i] == -1) {
            player_fds[i] = c;
            break;
        }
    }

pthread_mutex_unlock(&mtx);

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

static ssize_t writen_all(int fd, MessServer *mess) {

    size_t off = 0;

    while (off < sizeof(MessServer)) {

        ssize_t w = send(fd,
                         ((char*)mess) + off,
                         sizeof(MessServer) - off,
                         0);

        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }

        off += w;
    }

    return off;
}

static ssize_t readn_all(int fd, void *buf, size_t len){
    size_t off = 0;

    while (off < len) {

        ssize_t r = recv(
            fd,
            ((char*)buf) + off,
            len - off,
            0
        );

        if (r == 0)
            return 0;

        if (r < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        off += r;
    }

    return off;
}

int check_game_over() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (now.tv_sec > end_time.tv_sec ||
       (now.tv_sec == end_time.tv_sec &&
        now.tv_nsec >= end_time.tv_nsec)) {
        return 1;
    }

    return 0;
}

void addPlayer(Player* p){
    for(int i = 0; i < NUM_PLAYERS; i++){
        if(players[i] == NULL){
            players[i] = p;
            break;
        }
    }
}

void broadcast_game_over(Statistiche* vincitore) {
    MessServer msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_GAME_OVER;

    int fds_locali[NUM_PLAYERS];
    msg.p.lettera = vincitore->id;

    pthread_mutex_lock(&mtx);
    for (int i = 0; i < NUM_PLAYERS; i++) {
        fds_locali[i] = player_fds[i]; // Copiamo i descrittori nell'array locale
        if (player_fds[i] != -1) {
            send(player_fds[i], &msg, sizeof(msg), MSG_NOSIGNAL);
            player_fds[i] = -1; // Sanatizziamo subito l'array globale 
        }
    }
    pthread_mutex_unlock(&mtx);

    // pulizia sui socket
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (fds_locali[i] != -1) {
            shutdown(fds_locali[i], SHUT_RDWR); 
            close(fds_locali[i]);
        }
    }
}


void calcoloStatistiche(Statistiche* stats, Mappa* mappaGlobale) {

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            char cella = mappaGlobale->mappaPlayer[i][j];
                if (stats->id == cella) {
                    stats->celleConquistate++;            
                }
        }
    }

}