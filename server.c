#define MAIN_PROGRAM
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include "game.h" 

void addPlayer(Player* p);

void *timer_thread(void *arg) {
    
    (void)arg;
    int secondi_passati = 0;
    int durata_partita = 140; 
    int T = 20;               // Invia la mappa globale ogni tot sec
    Statistiche vincitore = { .id = '\0', .username = {0}, .celleConquistate = 0 };


    while (secondi_passati < durata_partita) {
        
        sleep(1);
        secondi_passati++;

        // Ogni T secondi invia la mappa globale a tutti (Richiesta della specifica)
        if (secondi_passati % T == 0 && !game_over) {
            pthread_mutex_lock(&mtx);
            
            MessServer msg_periodico;
            memset(&msg_periodico, 0, sizeof(msg_periodico));
            msg_periodico.type = MSG_GLOBAL_UPDATE;
            vincitore.id = '\0';
            vincitore.celleConquistate = -1; // Inizia da -1 per catturare anche chi ha 0 celle
            strcpy(vincitore.username, "Nessuno");

            Statistiche statistics[NUM_PLAYERS];
            memset(statistics, 0, sizeof(statistics));
            
            for (int i = 0; i < NUM_PLAYERS; i++) {

                Statistiche stat;
                memset(&stat, 0, sizeof(Statistiche));

                if (players[i] != NULL) {
                    stat.id = players[i]->lettera;
                    stat.celleConquistate = 0;
                    strcpy(stat.username, players[i]->username);
                    calcoloStatistiche(&stat, &mappaGlobale);
                } else {
                    stat.id = '\0';
                    stat.celleConquistate = 0;
                    strcpy(stat.username, "");
                    }

                statistics[i] = stat;
            }

            for(int i = 0; i < NUM_PLAYERS; i++){
                
                msg_periodico.statistics[i] = statistics[i];
                if (!players[i]) {
                    statistics[i].id = '\0'; 
                    statistics[i].celleConquistate = 0; 
                    strcpy(statistics[i].username, "");
                    continue;
                }
               

                if (msg_periodico.statistics[i].celleConquistate > vincitore.celleConquistate) {
                        vincitore.id = msg_periodico.statistics[i].id;
                        vincitore.celleConquistate = msg_periodico.statistics[i].celleConquistate;
                        strcpy(vincitore.username, msg_periodico.statistics[i].username);
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
                    writen_all(player_fds[i], &msg_periodico);
                }
            }
            pthread_mutex_unlock(&mtx);
        }
    }
        

    // Tempo scaduto!
    atomic_store(&game_over, true);

    pthread_mutex_lock(&mtx);
    if (vincitore.id == '\0' || vincitore.username[0] == '\0')
        strcpy(vincitore.username, "Nessuno collegato");
    pthread_mutex_unlock(&mtx);


    broadcast_game_over(&vincitore);
    _exit(0);
}

static void *handle_client(void *arg) {
    int fd = *(int *)arg;
    free(arg);
    char buf[N*N];
    Mappa mappaLocale;
    Player *p = malloc(sizeof(Player));
    memset(p, 0, sizeof(Player));
    bool autenticato= false;
    bool inizializzato = false;

    
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



        bool uscita = false;

        if(messClient.type == MSG_SUBSCRIBE && !autenticato) {
            messClient.username[31] = '\0';
            messClient.password[31] = '\0';

            if(!registraUtente(messClient.username, messClient.password)){
                MessServer messServer;
                memset(&messServer, 0, sizeof(messServer));

                messServer.type = MSG_SUBSCRIBE;
                strcpy(messServer.p.username, "FAIL");
            
                if(writen_all(fd, &messServer)<0){ 
                    perror("send"); break; 
                }

                uscita = true;
                continue;
            }

            memcpy(p->username, messClient.username, 32);
            memcpy(p->password, messClient.password, 32);
            printf("Username player appena iscritto: %s ", p->username);
            autenticato = true;
        
            MessServer messServer;
            memset(&messServer, 0, sizeof(messServer));
            messServer.type = MSG_SUBSCRIBE;
            if(writen_all(fd, &messServer)<0){ 
                perror("send"); break; 
            }
        
        }
    

        if(messClient.type == MSG_LOGIN && !autenticato){
            messClient.username[31] = '\0';
            messClient.password[31] = '\0';

            if(!verificaCredenziali(messClient.username, messClient.password)){
                MessServer messServer;
                memset(&messServer, 0, sizeof(messServer));

                messServer.type = MSG_LOGIN;
                strcpy(messServer.p.username, "FAIL");
            
                if(writen_all(fd, &messServer)<0){ 
                    perror("send"); break; 
                }

                uscita = true;
                continue; 
            }
            memcpy(p->username, messClient.username, 32);
            memcpy(p->password, messClient.password, 32);
            autenticato = true;

            MessServer messServer;
            memset(&messServer, 0, sizeof(messServer));
            messServer.type = MSG_LOGIN;
            if(writen_all(fd, &messServer)<0){ 
                perror("send"); break; 
            }
        
        }

        if(!autenticato)
            continue;

        if(!inizializzato){
            unsigned int seed = (unsigned int)time(NULL) ^ (unsigned long)pthread_self();

            // Inizializza tutta la mappa locale con caratteri spazio ' '
            memset(mappaLocale.mappa, ' ', sizeof(mappaLocale.mappa));
            memset(mappaLocale.mappaPlayer, ' ', sizeof(mappaLocale.mappaPlayer));

            int num_simboli = sizeof(simboli) / sizeof(simboli[0]);
            char lettera_random = simboli[rand_r(&seed) % num_simboli];
            Colore colore_random = (Colore)((rand_r(&seed) % 12) + 4);
    
    
            
            p->lettera = lettera_random;
            p->riga = 5;
            p->colonna = 7; 
            p->colorePlayer = colore_random;

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

            inizializzato = true;
        }
    


        pthread_mutex_lock(&mtx);
        if (messClient.movimento) {
            uscita = invioMappaLocale(p, &mappaLocale, &mappaGlobale, messClient.direzione);
        } else {
            rivelaNebbia(p, &mappaLocale, &mappaGlobale);    
        }
        pthread_mutex_unlock(&mtx);

        if(uscita)
            break;  //uscita da while

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
    free(p);
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

void addPlayer(Player* p){
    for(int i = 0; i < NUM_PLAYERS; i++){
        if(players[i] == NULL){
            players[i] = p;
            printf("Aggiunto player: %s",players[i]->username);
            printf("Aggiunto player: %c",players[i]->lettera);
            break;
        }
    }
}