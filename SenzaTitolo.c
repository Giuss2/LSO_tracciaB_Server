#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h> 

#define N 20
#define MURO (char)254

typedef enum {
    RESET_COLOR,
    BIANCO,
    BLACK,
    GRIGIO,
    ROSSO,
    VERDE,
    GIALLO,
    BLU,
    MAGENTA,
    CIANO,
    ROSSO_CHIARO,
    VERDE_CHIARO,
    GIALLO_CHIARO,
    BLU_CHIARO,
    MAGENTA_CHIARO,
    CIANO_CHIARO
} Colore;

const char *colori[] = {
    "\x1b[0m",    // RESET
    "\x1b[47m",   // BIANCO background
    "\x1b[40m",   // BLACK background
    "\x1b[100m",  // GRIGIO background
    "\x1b[41m",   // ROSSO background
    "\x1b[42m",   // VERDE background
    "\x1b[43m",   // GIALLO background
    "\x1b[44m",   // BLU background
    "\x1b[45m",   // MAGENTA background
    "\x1b[46m",   // CIANO background

    "\x1b[101m",  // ROSSO CHIARO background
    "\x1b[102m",  // VERDE CHIARO background
    "\x1b[103m",  // GIALLO CHIARO background
    "\x1b[104m",  // BLU CHIARO background
    "\x1b[105m",  // MAGENTA CHIARO background
    "\x1b[106m"   // CIANO CHIARO background
};

typedef struct player {
    char lettera;
    int riga;
    int colonna;
    Colore colorePlayer;
} Player;

typedef struct mappa {
    char mappa[N][N];
    char mappaPlayer[N][N];
} Mappa;

Colore getColoreCasella(Player p, int i, int j, char mappaPlayer[N][N], char mappa[N][N]);

void rivelaNebbia(Player p, char mappa[N][N], char mappaGlobale[N][N]);

bool verificaMossa(int riga, int colonna, char mappa[N][N]);

void stampaMappa(Player p, char mappa[N][N], char mappaPlayer[N][N]);

int main() {

    char cella_libera = 250;

    Mappa mappaLocale;
    Mappa mappa;

    char simboli[] = {
        cella_libera,
        MURO,

        'A','B','C','D','E','F','G','H','I','J',
        'K','L','M','N','O','P','Q','R','S','T',
        'U','V','W','X','Y','Z',

        'a','b','c','d','e','f','g','h','i','j',
        'k','l','m','n','o','p','q','r','s','t',
        'u','v','w','x','y','z'
    };

    int num_simboli = sizeof(simboli) / sizeof(simboli[0]);

    for(int i = 0; i < N; i++) {
        for(int j = 0; j < N; j++) {
            mappaLocale.mappa[i][j] = ' ';
        }
    }

    srand(time(NULL));

    for(int i = 0; i < N; i++) {
        for(int j = 0; j < N; j++) {

            if((rand() % 100) > 20)
                mappa.mappa[i][j] = simboli[0];
            else
                mappa.mappa[i][j] = simboli[1];

            mappaLocale.mappaPlayer[i][j] = '0';
        }
    }

    char lettera_random = simboli[rand() % num_simboli];

    // VERSIONE C PURA
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

        if(mappa.mappa[posizione_riga][posizione_colonna] == cella_libera) {
            p.colonna = posizione_colonna;
            p.riga = posizione_riga;
        }

    } while(mappa.mappa[posizione_riga][posizione_colonna] != cella_libera);

    mappa.mappa[p.riga][p.colonna] = p.lettera;
    mappaLocale.mappa[p.riga][p.colonna] = p.lettera;

    mappa.mappaPlayer[p.riga][p.colonna] = p.lettera;
    mappaLocale.mappaPlayer[p.riga][p.colonna] = p.lettera;

    stampaMappa(p,
                mappaLocale.mappa,
                mappaLocale.mappaPlayer);

    while(true) {

        printf("Premi un tasto:\n");

        char c = getchar();

        while(getchar() != '\n');

        printf("\e[1;1H\e[2J");

        int riga_nuova = p.riga;
        int colonna_nuova = p.colonna;

        switch(c) {

            case 'A':
            case 'a':
                colonna_nuova--;
                break;

            case 'S':
            case 's':
                riga_nuova++;
                break;

            case 'W':
            case 'w':
                riga_nuova--;
                break;

            case 'D':
            case 'd':
                colonna_nuova++;
                break;
        }

        rivelaNebbia(p,
                      mappaLocale.mappa,
                      mappa.mappa);

        if(verificaMossa(riga_nuova,
                         colonna_nuova,
                         mappa.mappa)) {

            mappaLocale.mappa[p.riga][p.colonna] = cella_libera;

            p.colonna = colonna_nuova;
            p.riga = riga_nuova;

            mappaLocale.mappaPlayer[p.riga][p.colonna] = p.lettera;

            mappaLocale.mappa[p.riga][p.colonna] = p.lettera;

            rivelaNebbia(p,
                          mappaLocale.mappa,
                          mappa.mappa);
        }

        stampaMappa(p,
                    mappaLocale.mappa,
                    mappaLocale.mappaPlayer);
    }

    return 0;
}

Colore getColoreCasella(Player p,
                        int i,
                        int j,
                        char mappaPlayer[N][N],
                        char mappa[N][N]) {

    if(mappa[i][j] == ' ')
        return GRIGIO;

    else if(mappaPlayer[i][j] == p.lettera)
        return p.colorePlayer;

    else if(mappa[i][j] == MURO)
        return BIANCO;

    else
        return BLACK;
}

void rivelaNebbia(Player p,
                  char mappa[N][N],
                  char mappaGlobale[N][N]) {

    for(int i = p.riga - 1; i <= p.riga + 1; i++) {

        for(int j = p.colonna - 1; j <= p.colonna + 1; j++) {

            if(j < 0 || i < 0 || j > N - 1 || i > N - 1)
                continue;

            if(mappa[i][j] == ' ')
                mappa[i][j] = mappaGlobale[i][j];
        }
    }
}

void stampaMappa(Player p,
                 char mappa[N][N],
                 char mappaPlayer[N][N]) {

    for(int i = 0; i < N; i++) {

        for(int j = 0; j < N; j++) {

            Colore colore =
                getColoreCasella(p,
                                 i,
                                 j,
                                 mappaPlayer,
                                 mappa);

            printf("%s%c %s",
                   colori[colore],
                   mappa[i][j],
                   colori[0]);
        }

        printf("\n");
    }
}

bool verificaMossa(int riga,
                   int colonna,
                   char mappa[N][N]) {

    if(colonna < 0 || riga < 0 ||
       colonna > N - 1 || riga > N - 1)
        return false;

    if(mappa[riga][colonna] == MURO)
        return false;

    return true;
}