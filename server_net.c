#include "mappa.h"
#include <sys/socket.h>
#include <errno.h>
#include <string.h>


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

/*
void build_global_update_message(MessServer *msg, Statistiche *vincitore) {
    memset(msg, 0, sizeof(MessServer));
    msg->type = MSG_GLOBAL_UPDATE;

    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (players[i]) {
            msg->statistics[i].id = players[i]->lettera;
            calcoloStatistiche(&msg->statistics[i], &mappaGlobale);
            
            if (msg->statistics[i].celleConquistate > vincitore->celleConquistate) {
                vincitore->id = msg->statistics[i].id;
                vincitore->celleConquistate = msg->statistics[i].celleConquistate;
            }
            msg->players[i] = *players[i];
        }
    }

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            msg->mappaPlayer.mappa[i][j] = (mappaGlobale.mappa[i][j] == MURO) ? '.' : mappaGlobale.mappa[i][j];
            msg->mappaPlayer.mappaPlayer[i][j] = mappaGlobale.mappaPlayer[i][j];
        }
    }
}
    */