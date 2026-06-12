#include "game.h"
#include <stdlib.h>


void rivelaNebbia(Player *p, Mappa* mappa, Mappa* mappaGlobale){

    for(int i = p->riga - 1; i <= p->riga + 1; i++) {
        for(int j = p->colonna - 1; j <= p->colonna + 1; j++) {

            if(j < 0 || i < 0 || j > N - 1 || i > N - 1)
                continue;

            mappa->mappa[i][j] = mappaGlobale->mappa[i][j];
            mappa->mappaPlayer[i][j] = mappaGlobale->mappaPlayer[i][j];
        }
    }
}

bool verificaMossa(int riga, int colonna, char mappa[N][N]) {

    if(colonna < 0 || riga < 0 ||colonna > N - 1 || riga > N - 1)
        return false;

    if(mappa[riga][colonna] == MURO)
        return false;
    
    if(mappa[riga][colonna] != cella_libera)
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
}

void calcoloStatistiche(Statistiche* stats, Mappa* mappaGlobale) {
    stats->celleConquistate = 0;
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            if (stats->id == mappaGlobale->mappaPlayer[i][j]) {
                stats->celleConquistate++;
            }
        }
    }
}