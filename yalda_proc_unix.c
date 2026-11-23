/// ₍ᐢ. .ᐢ₎ ₊˚⊹
/// @file yalda_proc_unix.c
/// @brief Linux/Unix process scanner and Discord IPC socket stub.
/// @author adiyn

// TODO

#include "yalda_connection.h"

#ifdef __linux__

int ConnectionDiscord(void)
{
    return -1;
}

bool Disconnect(int sockFd)
{
    if (sockFd >= 0) {
        close(sockFd);
        return true;
    }
    return false;
}

char** getProcsNames(void)
{
    char** empty = malloc(sizeof(char*));
    if (empty) {
        empty[0] = NULL;
    }
    return empty;
}

void freeProcs(char** procs)
{
    if (!procs) return;
    for (size_t i = 0; procs[i] != NULL; i++) {
        free(procs[i]);
    }
    free(procs);
}

#endif /* __linux__ */