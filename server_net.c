#include "game.h"
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

ssize_t writen_all(int fd, MessServer *mess) {
    size_t off = 0;

    while (off < sizeof(MessServer)) {
        ssize_t w = send(fd, ((char*)mess) + off, sizeof(MessServer) - off, 0);
        
        if (w < 0) { 
            if (errno == EINTR) 
                continue; 
            return -1; 
        }
        off += w;
    }
    return off;
}

ssize_t readn_all(int fd, void *buf, size_t len) {
    size_t off = 0;

    while (off < len) {
        ssize_t r = recv(fd, ((char*)buf) + off, len - off, 0);
        
        if (r == 0) return 0;
        
        if (r < 0) { 
            if (errno == EINTR) 
                continue; 
            return -1; 
        }
        off += r;
    }
    return off;
}


void broadcast_game_over(Statistiche* vincitore) {
    MessServer msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_GAME_OVER;

    int fds_locali[NUM_PLAYERS];
    
    strcpy(msg.p.username, vincitore->username);

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

bool registraUtente(char username[32], char password[32]){

    pthread_mutex_lock(&file_mtx);
    int fd = open("credenziali_utenti.txt", O_RDWR | O_CREAT | O_APPEND, S_IRWXU | S_IRGRP | S_IROTH);
    if (fd < 0) {
        perror("Errore open credenziali_utenti.txt");
        pthread_mutex_unlock(&file_mtx);
        return false;
    }

    // due utenti non possono avere lo stesso username
    char riga[100];
    int idx = 0;
    char c;
    bool trovato = false;

    while (read(fd, &c, 1) > 0) {
        if (c != '\n' && idx < (int)sizeof(riga) - 1) {
            riga[idx++] = c;
        } else {
            riga[idx] = '\0'; // Fine riga corrente
            idx = 0;          // Reset per la prossima riga

            char *token = strtok(riga, ",");
            if (token != NULL && strcmp(token, username) == 0) {
                trovato = true;
                break; // Username duplicato
            }
        }
    }

    if (trovato) {
        printf("Registrazione fallita: lo username '%s' esiste già.\n", username);
        close(fd);
        pthread_mutex_unlock(&file_mtx);
        return false;
    }

    lseek(fd, 0, SEEK_END);
    size_t len_user = strlen(username);
    size_t len_pass = strlen(password);
    
    size_t totale_bytes = len_user + 1 + len_pass + 1; 
    
    char buffer[67]; 
    char *ptr_buf = buffer;

    memcpy(ptr_buf, username, len_user);
    ptr_buf += len_user; 
    *ptr_buf = ',';
    ptr_buf += 1;
    memcpy(ptr_buf, password, len_pass);
    ptr_buf += len_pass;
    *ptr_buf = '\n';
    
    char *ptr_write = buffer;
    size_t rimasti = totale_bytes;
    ssize_t n_written;

    while (rimasti > 0) {
        n_written = write(fd, ptr_write, rimasti);
        if (n_written <= 0) {
            if (n_written < 0 && errno == EINTR) continue; 
            perror("Errore write utenti.txt");
            break;
        }
        ptr_write += n_written;
        rimasti -= n_written;
    }

    close(fd);
    pthread_mutex_unlock(&file_mtx);    
    return true;

}

bool verificaCredenziali(char username[32], char password[32]){

    pthread_mutex_lock(&file_mtx);
    int fd = open("credenziali_utenti.txt", O_RDONLY | O_CREAT, S_IRUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        perror("Errore open credenziali_utenti.txt");
        pthread_mutex_unlock(&file_mtx);
        return false;
    }

    char riga[100];
    int idx = 0;
    char c;
    bool trovato = false;

    while (read(fd, &c, 1) > 0) {
        if (c != '\n' && idx < (int)sizeof(riga) - 1) {
            riga[idx++] = c;
        } else {
            riga[idx] = '\0'; // Fine riga corrente
            idx = 0;          // Reset per la prossima riga

            char *token = strtok(riga, ",");
            if (token != NULL && strcmp(token, username) == 0) {
                trovato = true;
                break;
            }
        }
    }

    if (!trovato) {
        printf("Login fallito: lo username '%s' non esiste.\n", username);
        close(fd);
        pthread_mutex_unlock(&file_mtx);
        return false;
    }

    close(fd);
    pthread_mutex_unlock(&file_mtx);

    return true;

}