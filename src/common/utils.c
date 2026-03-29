#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "utils.h"

int send_msg(int fd, const char *msg) {
    uint32_t len     = (uint32_t)strlen(msg);
    uint32_t net_len = htonl(len);          

    // send length header first 
    if (send(fd, &net_len, sizeof(net_len), 0) != sizeof(net_len))
        return -1;

    // send message body 
    if (send(fd, msg, len, 0) != (ssize_t)len)
        return -1;

    return 0;
}

int recv_msg(int fd, char *buf, int buf_size) {
    uint32_t net_len;

    
    // MSG_WAITALL tells the OS: don't return until you have
    // exactly sizeof(net_len) bytes. Without this, recv()
    // might return fewer bytes and we'd misread the length.

    if (recv(fd, &net_len, sizeof(net_len), MSG_WAITALL) <= 0)
        return -1;

    uint32_t len = ntohl(net_len); // network → host byte order 

    // safety: never write more than the buffer can hold 
    if ((int)len >= buf_size)
        len = (uint32_t)(buf_size - 1);

    if (recv(fd, buf, len, MSG_WAITALL) <= 0)
        return -1;

    buf[len] = '\0';
    return 0;
}