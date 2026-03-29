#ifndef SERVER_H
#define SERVER_H


// server_run — bind to port, accept clients, fork a child per client.
//              Never returns (runs until killed with Ctrl+C).

void server_run(void);

#endif