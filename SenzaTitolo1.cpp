

#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <stdlib.h>
#include <time.h>
#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define RESET   "\x1b[0m"
#define N 20
#define MURO (char)219

typedef struct player{
	char lettera;
	int riga;
	int colonna;
} Player;
//enum Direzione{ SINISTRA, DESTRA, GIU, SU};
            
bool verificaMossa(int riga, int colonna);			
			
char mappa[N][N];		
			 
int main(){
	//char MURO = 219;
	char cella_libera = 250;
	char cella_occupata = 254;
	
	
	char simboli[] = {cella_libera, MURO, cella_occupata, 'A','B','C','D','E','F','G', 'H','I','J','K','L','M','N', 'O','P','Q','R','S','T','U',
    'V','W','X','Y','Z', 'a','b','c','d','e','f','g', 'h','i','j','k','l','m','n', 'o','p','q','r','s','t','u', 'v','w','x','y','z'};
    
	int num_simboli = sizeof(simboli) / sizeof(simboli[0]);
	

	
	srand(time(NULL));
	
	
 	for(int i = 0; i < N; i++) {
        for(int j = 0; j < N; j++) {

            if((rand() % 100) > 20){
            	mappa[i][j] = simboli[0];
			}
			else
				mappa[i][j] = simboli[1];
        }
    }
    
    Player p = {'A', 5, 7};
    mappa[5][7] = 'A';
    
    //PRIMA STAMPA
      for(int i = 0; i < N; i++) {
        for(int j = 0; j < N; j++) {
            printf("%c ", mappa[i][j]);
        }
        printf("\n");
    }
	printf("\n \n \n");
    
    while(true){
    		
    	    printf("Premi un tasto: \n");
   			char c = getchar();
   			getchar();
			printf("\e[1;1H\e[2J");

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
			mappa[p.riga][p.colonna] = p.lettera;
		}
		
		  //SECONDA STAMPA
    	 for(int i = 0; i < N; i++) {
        	for(int j = 0; j < N; j++) {
            	printf("%c ", mappa[i][j]);
        	}
        	printf("\n");
    	}
					
	}
    

	
	return 0;
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
