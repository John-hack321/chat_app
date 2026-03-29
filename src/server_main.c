#include <stdio.h>
#include "server/server.h"

int main(void) {
    printf("\n  ===========================================\n");
    printf("  ||  one-on-one chat  —  server               ||\n");
    printf("  ||  SCS3304 / client-server model            ||\n");
    printf("   ==============================================\n\n");
    server_run();
    return 0;
}