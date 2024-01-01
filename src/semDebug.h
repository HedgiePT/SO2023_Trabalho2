#pragma once

// (C) João Ferreira, 2023.
// N.º Mec.: 95316

#include <stdlib.h>
#include "semaphore.h"


int semDownOrExit(unsigned int index, const char *reason)
{
    (void)reason;
    int ret;

    if ((ret = semDown (semgid, index)) == -1) {
        perror ("semaphor down access failed (CT)");
        exit (EXIT_FAILURE);
    }

    return ret;
}

int semUpOrExit(unsigned int index, const char *reason)
{
    (void)reason;
    int ret;

    if ((ret = semUp (semgid, index)) == -1) {
        perror ("semaphor down access failed (CT)");
        exit (EXIT_FAILURE);
    }

    return ret;
}
