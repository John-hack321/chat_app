#ifndef UTILS_H
#define UTILS_H


int send_msg(int fd, const char *msg); // sends one complete message over a socket   // it will return 0 on succes and -1 on failure
int recv_msg(int fd, char *buf, int buf_size); // receives one complete message over a socket // same as the other one

#endif