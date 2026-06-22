#ifndef MAPPA_H
#define MAPPA_H

#include <stdbool.h>    // Per bool, true, false
#include <stdatomic.h>  // Per atomic_bool
#include <pthread.h>
#include <sys/types.h>  // Per ssize_t, size_t
#include <unistd.h>     // Per le syscall di I/O

#define PORT        5201
#define BACKLOG      8
#define NUM_PLAYERS  12
#define N            30
#define MURO         'X'

typedef enum {
    RESET_COLOR, BIANCO, BLACK, GRIGIO, ROSSO, VERDE, GIALLO, BLU, MAGENTA, CIANO,
    ROSSO_CHIARO, VERDE_CHIARO, GIALLO_CHIARO, BLU_CHIARO, MAGENTA_CHIARO, CIANO_CHIARO
} Colore;


typedef enum {
    MSG_UPDATE = 0,
    MSG_GAME_OVER = 1,
    MSG_GLOBAL_UPDATE = 2,
    MSG_SUBSCRIBE = 3,
    MSG_LOGIN = 4
} MsgType;

typedef struct player {
    char lettera;
    int riga;
    int colonna;
    Colore colorePlayer;
    char username[32];
    char password[32];
    int fd;     // per il broadcast
} Player;

typedef struct mappa {
    char mappa[N][N];
    char mappaPlayer[N][N];
} Mappa;

typedef struct statistiche {
    char id;
    char username[32];
    int celleConquistate;
} Statistiche;

typedef struct messServer {
     Mappa mappaPlayer;
     Player p;
     Player players[NUM_PLAYERS];
     MsgType type;
     Statistiche statistics[NUM_PLAYERS];
     int secondi_rimanenti;

} MessServer;

typedef struct messClient {
    char direzione;
    bool movimento;
    char username[32];
    char password[32];
    MsgType type;
} MessClient;


#ifdef MAIN_PROGRAM
    // --- ALLOCAZIONE FISICA DELLE VARIABILI ---
    pthread_mutex_t mtx;
    pthread_mutex_t file_mtx;
    atomic_bool game_over = false;
    struct timespec end_time;
    Mappa mappaGlobale;
    Player* players[NUM_PLAYERS];
    int player_fds[NUM_PLAYERS];

    const char cella_libera = '.';
    const char *colori[] = {
        "\x1b[0m", "\x1b[47m", "\x1b[40m", "\x1b[100m", "\x1b[41m", "\x1b[42m", 
        "\x1b[43m", "\x1b[44m", "\x1b[45m", "\x1b[46m", "\x1b[101m", "\x1b[102m", 
        "\x1b[103m", "\x1b[104m", "\x1b[105m", "\x1b[106m"
    };
    
    char simboli[] = {
        'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','Y','Z',
        'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','y','z'
    };
#else
    // --- RIFERIMENTO EXTERN ---
    extern pthread_mutex_t mtx;
    extern pthread_mutex_t file_mtx;
    extern atomic_bool game_over;
    extern struct timespec end_time;
    extern Mappa mappaGlobale;
    extern Player* players[NUM_PLAYERS];
    extern int player_fds[NUM_PLAYERS];
    extern const char cella_libera;
    extern const char *colori[];
    extern char simboli[];
#endif


// PROTOTIPI 
void rivelaNebbia(Player *p, Mappa* mappa, Mappa* mappaGlobale);
bool verificaMossa(int riga, int colonna, char mappa[N][N]);
int check_game_over();
ssize_t writen_all(int fd, MessServer *mess);
ssize_t readn_all(int fd, void *buf, size_t len);
bool invioMappaLocale(Player *p, Mappa *mappaLocale, Mappa *mappa, char direzione);
void addPlayer(Player* p);
void broadcast_game_over(Statistiche* vincitore);
void calcoloStatistiche(Statistiche* stats, Mappa* mappaGlobale);
bool registraUtente(char username[32], char password[32]);
bool verificaCredenziali(char username[32], char password[32]);

#endif