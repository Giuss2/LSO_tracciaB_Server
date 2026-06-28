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
volatile sig_atomic_t server_running = 1;
int counter = 0;
int main_socket_fd;

int secondi_passati = 0;
int durata_partita = 140; 

void addPlayer(Player* p);

void handler(int signo){
    counter++;
    if(signo == SIGINT && counter>=3){
            server_running = 0;
            close(main_socket_fd);
            
    }
}


void *timer_thread(void *arg) {

    struct sockaddr_in addr;
    int sock_broadcast= socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_broadcast < 0) { perror("socket broadcast"); exit(1);}

    int yes = 1;
    if (setsockopt(sock_broadcast, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes)) < 0) {
        perror("SO_BROADCAST"); exit(1); 
    }
    // Imposta indirizzo broadcast destinazione
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = inet_addr(BROADCAST_ADDR);
    
    (void)arg;

    int T = 20;               // Invia la mappa globale ogni tot sec
    Statistiche vincitore = { .id = '\0', .username = {0}, .celleConquistate = 0 };


    while (secondi_passati < durata_partita) {
        
        sleep(1);
        secondi_passati++;

        // Ogni T secondi invia la mappa globale a tutti (Richiesta della specifica)
        if (secondi_passati % T == 0 && !game_over) {
            pthread_mutex_lock(&mtx);
            
            MessDaInviare msg_periodico;
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
                    if(mappaGlobale.planciaDiGioco[i][j] == MURO)
                        msg_periodico.mappa.planciaDiGioco[i][j] = cella_libera;
                    else{
                        msg_periodico.mappa.planciaDiGioco[i][j] = mappaGlobale.planciaDiGioco[i][j];
                    }                    
                    msg_periodico.mappa.territorioGiocatori[i][j] = mappaGlobale.territorioGiocatori[i][j];
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
    MessBroadcast msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_GAME_OVER;

    pthread_mutex_lock(&mtx);
    if (vincitore.id == '\0' || vincitore.username[0] == '\0')
        strcpy(vincitore.username, "Nessuno collegato");
    
    strcpy(msg.p.username, vincitore.username);
    
    pthread_mutex_unlock(&mtx);

    if (sendto(sock_broadcast, &msg, sizeof(MessBroadcast), 0, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Errore nell'invio del broadcast di GAME OVER");
        exit(1);
    }

    // invia anche a socket tcp che è terminata la partita
    pthread_mutex_lock(&mtx);
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (player_fds[i] != -1) {
            MessDaInviare msg_tcp; 
            memset(&msg_tcp, 0, sizeof(msg_tcp));
            msg_tcp.type = MSG_GAME_OVER;
            strcpy(msg_tcp.p.username, vincitore.username);
        
            writen_all(player_fds[i], &msg_tcp); 
        }
    }
    pthread_mutex_unlock(&mtx);

    close(sock_broadcast);
    exit(0);
    pthread_exit(NULL);
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

    
    MessRicevuto messRicevuto;
    while (!game_over) {
        if (atomic_load(&game_over))
            break;

        ssize_t n = readn_all(fd, &messRicevuto, sizeof(messRicevuto));
        if (n == 0)
            break;

        if (n < 0) {
            perror("recv");
            break;
        }



        bool uscita = false;

        if(messRicevuto.type == MSG_SUBSCRIBE && !autenticato) {
            messRicevuto.username[31] = '\0';
            messRicevuto.password[31] = '\0';

            if(!registraUtente(messRicevuto.username, messRicevuto.password)){
                MessDaInviare messDaInviare;
                memset(&messDaInviare, 0, sizeof(messDaInviare));

                messDaInviare.type = MSG_SUBSCRIBE;
                strcpy(messDaInviare.p.username, "FAIL");
            
                if(writen_all(fd, &messDaInviare)<0){ 
                    perror("send"); break; 
                }

                uscita = true;
                continue;
            }

            memcpy(p->username, messRicevuto.username, 32);
            memcpy(p->password, messRicevuto.password, 32);
            autenticato = true;
        
            MessDaInviare messDaInviare;
            memset(&messDaInviare, 0, sizeof(messDaInviare));
            messDaInviare.type = MSG_SUBSCRIBE;
            if(writen_all(fd, &messDaInviare)<0){ 
                perror("send"); break; 
            }
        
        }
    

        if(messRicevuto.type == MSG_LOGIN && !autenticato){
            messRicevuto.username[31] = '\0';
            messRicevuto.password[31] = '\0';

            if(!verificaCredenziali(messRicevuto.username, messRicevuto.password)){
                MessDaInviare messDaInviare;
                memset(&messDaInviare, 0, sizeof(messDaInviare));

                messDaInviare.type = MSG_LOGIN;
                strcpy(messDaInviare.p.username, "FAIL");
            
                if(writen_all(fd, &messDaInviare)<0){ 
                    perror("send"); break; 
                }

                uscita = true;
                continue; 
            }


            memcpy(p->username, messRicevuto.username, 32);
            memcpy(p->password, messRicevuto.password, 32);
            autenticato = true;

            MessDaInviare messDaInviare;
            memset(&messDaInviare, 0, sizeof(messDaInviare));
            messDaInviare.type = MSG_LOGIN;
            if(writen_all(fd, &messDaInviare)<0){ 
                perror("send"); break; 
            }
        
        }

        if(!autenticato)
            continue;

        if(!inizializzato){
            unsigned int seed = (unsigned int)time(NULL) ^ (unsigned long)pthread_self();

            // Inizializza tutta la mappa locale con caratteri spazio ' '
            memset(mappaLocale.planciaDiGioco, ' ', sizeof(mappaLocale.planciaDiGioco));
            memset(mappaLocale.territorioGiocatori, ' ', sizeof(mappaLocale.territorioGiocatori));

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

                if(mappaGlobale.planciaDiGioco[posizione_riga][posizione_colonna] == cella_libera) {
                    p->colonna = posizione_colonna;
                    p->riga = posizione_riga;
                }

            } while(mappaGlobale.planciaDiGioco[posizione_riga][posizione_colonna] != cella_libera);

            mappaGlobale.planciaDiGioco[p->riga][p->colonna] = p->lettera;
            mappaLocale.planciaDiGioco[p->riga][p->colonna] = p->lettera;

            mappaGlobale.territorioGiocatori[p->riga][p->colonna] = p->lettera;
            mappaLocale.territorioGiocatori[p->riga][p->colonna] = p->lettera;

            pthread_mutex_unlock(&mtx);

            inizializzato = true;
        }
    


        pthread_mutex_lock(&mtx);
        if (messRicevuto.type == MSG_MOVE) {
            uscita = invioMappaLocale(p, &mappaLocale, &mappaGlobale, messRicevuto.direzione);
        } else {
            rivelaNebbia(p, &mappaLocale, &mappaGlobale);    
        }
        pthread_mutex_unlock(&mtx);

        if(uscita)
            break;  //uscita da while

        MessDaInviare messDaInviare;
        messDaInviare.p = *p;
        messDaInviare.mappa = mappaLocale;
        messDaInviare.type = MSG_UPDATE;

        pthread_mutex_lock(&mtx);
        for (int i = 0; i < NUM_PLAYERS; i++) {
            if (players[i])
                messDaInviare.players[i] = *players[i];
            else
                messDaInviare.players[i].lettera = '\0';
        }
        pthread_mutex_unlock(&mtx);


        if (writen_all(fd, &messDaInviare)<0) {
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

    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (players[i] == p) {
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

    Player* p = NULL;

    pthread_mutex_init(&mtx, NULL);
    pthread_mutex_init(&file_mtx, NULL);

    signal(SIGPIPE, SIG_IGN);


    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return 1; }
    main_socket_fd = s;

    int opt = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        close(s);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(TCP_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(s, BACKLOG) < 0) { perror("listen"); return 1; }

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    end_time.tv_sec += 30000; // durata partita

    pthread_t timer;
    pthread_create(&timer, NULL, timer_thread, NULL);
    pthread_detach(timer);


    //printf("Server in ascolto su 192.168.1.187:%d (thread per connessione)\n", TCP_PORT);
    

    if (signal(SIGINT, handler) == SIG_ERR) {
        perror("signal");
        exit(1);
    }   
        

    int num_simboli = sizeof(simboli) / sizeof(simboli[0]);

    unsigned int seed = time(NULL) ^ pthread_self();

    rand_r(&seed);

    pthread_mutex_lock(&mtx);
    for(int i = 0; i < N; i++) {
        for(int j = 0; j < N; j++) {
            if((rand_r(&seed) % 100) > 20)
                mappaGlobale.planciaDiGioco[i][j] = cella_libera;
            else
                mappaGlobale.planciaDiGioco[i][j] = MURO;
        }
    }

    for(int i=0; i < NUM_PLAYERS; i++){
        players[i] = NULL;
        player_fds[i] = -1;
    }
    pthread_mutex_unlock(&mtx);

    while(server_running) {
        int c = accept(s, NULL, NULL);
        if (c < 0) { 
            if (errno == EBADF) break;
            if (errno == EINTR) continue; 
            perror("accept"); continue; 
        }

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
            perror("pthread_create");
            close(c);
            free(fdp);
            continue;
        }
        pthread_detach(tid);
    }

   pthread_mutex_lock(&mtx);
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (player_fds[i] != -1) {
        // invia un messaggio di "Server in chiusura" ai client
            close(player_fds[i]);
            player_fds[i] = -1;
        }
    }
    pthread_mutex_unlock(&mtx);

    if(s>=0)
        close(s);
    
    pthread_mutex_destroy(&mtx);
    pthread_mutex_destroy(&file_mtx);
}

void addPlayer(Player* p){
    for(int i = 0; i < NUM_PLAYERS; i++){
        if(players[i] == NULL){
            players[i] = p;
            break;
        }
    }
}