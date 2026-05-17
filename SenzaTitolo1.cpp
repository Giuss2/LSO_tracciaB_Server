#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <stdlib.h>
#include <time.h>
#define N 20
#define MURO (char)254

typedef enum {
	RESET_COLOR,
	BIANCO,
	BLACK,
    ROSSO,
    VERDE,
    GIALLO,
    BLU
    
    
} Colore;
const char *colori[] = {
    "\x1b[0m",   // RESET
    "\x1b[47m",  // BIANCO background
    "\x1b[40m", // BLACK background
	"\x1b[41m", // ROSSO background
    "\x1b[42m", // VERDE background
    "\x1b[43m", // GIALLO background
    "\x1b[44m" // BLU background
    
    
};
    
    
typedef struct player{
	char lettera;
	int riga;
	int colonna;
	Colore colorePlayer;
} Player;
        
Colore getColoreCasella(Player p, int i, int j, char mappaPlayer[N][N], char mappa[N][N]);
bool verificaMossa(int riga, int colonna);			
void stampaMappa(Player p, char mappa[N][N], char mappaPlayer[N][N]);
char mappa[N][N];
char mappaPlayer[N][N];	
			 
int main(){
	char cella_libera = 250;
	
	
	char simboli[] = {cella_libera, MURO, 'A','B','C','D','E','F','G', 'H','I','J','K','L','M','N', 'O','P','Q','R','S','T','U',
    'V','W','X','Y','Z', 'a','b','c','d','e','f','g', 'h','i','j','k','l','m','n', 'o','p','q','r','s','t','u', 'v','w','x','y','z'};
    
	int num_simboli = sizeof(simboli) / sizeof(simboli[0]);
	

	
	srand(time(NULL));
	
	
 for(int i = 0; i < N; i++) {
    for(int j = 0; j < N; j++) {

        if((rand() % 100) > 20){
            mappa[i][j] = simboli[0];
        }
        else {
            mappa[i][j] = simboli[1];
        }
        mappaPlayer[i][j] = '0';
    }
}

	char lettera_random = simboli[rand() % num_simboli];
	Colore colore_random = static_cast<Colore>((rand() % 4) + 3);

    Player p = {lettera_random, 5, 7, colore_random};
    int posizione_riga;
    int posizione_colonna;
    
    do{
    	
    	posizione_riga = rand() % N;
    	posizione_colonna = rand() % N;
    	
    	if(mappa[posizione_riga][posizione_colonna] == cella_libera){
    		p.colonna = posizione_colonna;
    		p.riga = posizione_riga;
		}
    	
    	
	}while(mappa[posizione_riga][posizione_colonna] != cella_libera);
	
    
    mappa[p.riga][p.colonna] = p.lettera;
    mappaPlayer[p.riga][p.colonna] = p.lettera;
    
    //PRIMA STAMPA
      stampaMappa(p, mappa, mappaPlayer);
	
    
    while(true){
    		
    	    printf("Premi un tasto: \n");
   			char c = getchar();
   			while(getchar() != '\n');
			printf("\e[1;1H\e[2J");	//resetta stampa mappa

			int riga_nuova = p.riga;
			int colonna_nuova = p.colonna;
			

    		switch(c){
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
			
		if(verificaMossa(riga_nuova, colonna_nuova)){
			mappa[p.riga][p.colonna] = cella_libera;
			p.colonna = colonna_nuova;
			p.riga = riga_nuova;
			mappaPlayer[p.riga][p.colonna] = p.lettera;
			mappa[p.riga][p.colonna] = p.lettera;
		}
		
		  //SECONDA STAMPA
    	 stampaMappa(p, mappa, mappaPlayer);
					
	}
    


	return 0;
}


Colore getColoreCasella(Player p, int i, int j, char mappaPlayer[N][N], char mappa[N][N]){
	if(mappaPlayer[i][j]== p.lettera){
		return p.colorePlayer;
	}else if(mappa[i][j] == MURO){
		return BIANCO;
	}else
		return BLACK;
	
}

void stampaMappa(Player p, char mappa[N][N], char mappaPlayer[N][N]){
		for(int i = 0; i < N; i++) {
        	for(int j = 0; j < N; j++) {
        		
        		 		Colore colore = getColoreCasella(p, i, j, mappaPlayer, mappa);
        		 		printf("%s%c %s", colori[colore], mappa[i][j], colori[0]);
 
			}
			printf("\n");
        }
}


bool verificaMossa(int riga, int colonna){
	//sei sui bordi
	if(colonna<0 || riga<0 || colonna > N-1 || riga > N-1)
		return false;
		
	//controlla se ci sono muri
	if(mappa[riga][colonna]==MURO)
		return false;
		
	return true;
}
