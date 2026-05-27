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

#define PORT    5201
#define BACKLOG 8
Mappa mappaGlobale;

typedef struct messServer{
     Mappa mappaPlayer;
     Player p;
}MessServer;

typedef struct messClient{
    char direzione;
    bool movimento;
}MessClient;

void invioMappaLocale(Player *p, Mappa *mappaLocale, Mappa *mappa, char direzione);

static void *handle_client(void *arg) {
    int fd = *(int *)arg;
    free(arg);
    char buf[N*N];
    Mappa mappaLocale;

    
    // Inizializza tutta la mappa locale con caratteri spazio ' '
    memset(mappaLocale.mappa, ' ', sizeof(mappaLocale.mappa));
    memset(mappaLocale.mappaPlayer, ' ', sizeof(mappaLocale.mappaPlayer));

    int num_simboli = sizeof(simboli) / sizeof(simboli[0]);
    char lettera_random = simboli[rand() % num_simboli];
    Colore colore_random = (Colore)((rand() % 12) + 4);

    Player p = {
        lettera_random,
        5,
        7,
        colore_random
    };

    int posizione_riga;
    int posizione_colonna;

    do {

        posizione_riga = rand() % N;
        posizione_colonna = rand() % N;

        if(mappaGlobale.mappa[posizione_riga][posizione_colonna] == cella_libera) {
            p.colonna = posizione_colonna;
            p.riga = posizione_riga;
        }

    } while(mappaGlobale.mappa[posizione_riga][posizione_colonna] != cella_libera);

    mappaGlobale.mappa[p.riga][p.colonna] = p.lettera;
    mappaLocale.mappa[p.riga][p.colonna] = p.lettera;

    mappaGlobale.mappaPlayer[p.riga][p.colonna] = p.lettera;
    mappaLocale.mappaPlayer[p.riga][p.colonna] = p.lettera;

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
    // Solo per la prima mappa statica
        rivelaNebbia(&p, mappaLocale.mappa, mappaGlobale.mappa);
    }

    MessServer messServer;
    messServer.p = p;
    messServer.mappaPlayer = mappaLocale;  

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

    srand(time(NULL));

    for(int i = 0; i < N; i++) {
        for(int j = 0; j < N; j++) {

            if((rand() % 100) > 20)
                mappaGlobale.mappa[i][j] = simboli[0];
            else
                mappaGlobale.mappa[i][j] = simboli[1];

            //mappaLocale.mappaPlayer[i][j] = '0';
        }
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
}


void rivelaNebbia(Player *p, char mappa[N][N], char mappaGlobale[N][N]) {

    for(int i = p->riga - 1; i <= p->riga + 1; i++) {

        for(int j = p->colonna - 1; j <= p->colonna + 1; j++) {

            if(j < 0 || i < 0 || j > N - 1 || i > N - 1)
                continue;

            if(mappa[i][j] == ' ')
                mappa[i][j] = mappaGlobale[i][j];
        }
    }
}


bool verificaMossa(int riga, int colonna, char mappa[N][N]) {

    if(colonna < 0 || riga < 0 ||
       colonna > N - 1 || riga > N - 1)
        return false;

    if(mappa[riga][colonna] == MURO)
        return false;

    return true;
}

void invioMappaLocale(Player *p, Mappa *mappaLocale, Mappa *mappaGlobale, char direzione) {
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

    rivelaNebbia(p, mappaLocale->mappa, mappaGlobale->mappa);

    if(verificaMossa(riga_nuova, colonna_nuova, mappaGlobale->mappa)) {
        mappaLocale->mappa[p->riga][p->colonna] = cella_libera;
        mappaGlobale->mappa[p->riga][p->colonna] = cella_libera;

        p->colonna = colonna_nuova;
        p->riga = riga_nuova;

        mappaLocale->mappaPlayer[p->riga][p->colonna] = p->lettera;
        mappaGlobale->mappaPlayer[p->riga][p->colonna] = p->lettera;

        mappaLocale->mappa[p->riga][p->colonna] = p->lettera;
        mappaGlobale->mappa[p->riga][p->colonna] = p->lettera;

        rivelaNebbia(p, mappaLocale->mappa, mappaGlobale->mappa);
    }
}
