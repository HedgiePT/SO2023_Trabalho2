#pragma once

// (C) João Ferreira, 2023.
// N.º Mec.: 95316

#include <stdlib.h>
#include "semaphore.h"

#ifdef SEMDEBUG
#include <stdio.h>
#include <unistd.h>
#include "sharedDataSync.h"
#include "semDebug_sharedDataSync.h"

struct semdebug_buff *semdebug_channel = NULL;

void semdebug_init(struct semdebug_buff *ch)
{
    semdebug_channel = ch;
    ch->pid = getpid();
}

int semDownOrExit(unsigned int index, const char *reason)
{
    int ret;
    char name[100];

    if (!reason)
        reason = "(no reason provided)";

    get_semaphore_name(&sh->fSt, index, name, 100);

#ifdef SEMDEBUG_WRITE_TO_STDERR
    fprintf(stderr, "PID %d acquired semaphore ID %d (\"%s\"): %s\n",
            getpid(), index, name, reason
    );
#endif

    semdebug_writeEvToBuffer(semdebug_channel, SEMDEBUG_DOWN, index, reason);

    if ((ret = semDown (semgid, index)) == -1) {
        perror ("semaphor down access failed (CT)");
        exit (EXIT_FAILURE);
    }

    return ret;
}

int semUpOrExit(unsigned int index, const char *reason)
{
    int ret;
    char name[100];

    if (!reason)
        reason = "(no reason provided)";

    get_semaphore_name(&sh->fSt, index, name, 100);
#ifdef SEMDEBUG_WRITE_TO_STDERR
    fprintf(stderr, "PID %d released semaphore ID %d (\"%s\"): %s\n",
            getpid(), index, name, reason
    );
#endif

    semdebug_writeEvToBuffer(semdebug_channel, SEMDEBUG_UP, index, reason);

    if ((ret = semUp (semgid, index)) == -1) {
        perror ("semaphor up access failed (CT)");
        exit (EXIT_FAILURE);
    }

    return ret;
}

#else

int semDownOrExit(unsigned int index, const char *reason)
{
    int ret;

    if ((ret = semDown (semgid, index)) == -1) {
        perror ("semaphor down access failed (CT)");
        exit (EXIT_FAILURE);
    }

    return ret;
}

int semUpOrExit(unsigned int index, const char *reason)
{
    int ret;

    if ((ret = semUp (semgid, index)) == -1) {
        perror ("semaphor down access failed (CT)");
        exit (EXIT_FAILURE);
    }

    return ret;
}

#endif //SEMDEBUG
