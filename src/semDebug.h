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

static void semdebug_logEvent(struct semdebug_buff *ch,
                              enum SEMDEBUG_ACTION action,
                              unsigned int index, const char *reason)
{
    if (!reason)
        reason = "";

    semdebug_writeEvToBuffer(ch, action, index, reason);    

#ifdef SEMDEBUG_WRITE_TO_STDERR
    char name[100];
    get_semaphore_name(&sh->fSt, index, name, 100);
    fprintf(stdout, "PID %d %s semaphore ID %d (\"%s\"): %s\n",
            getpid(), action == SEMDEBUG_DOWN ? "acquired" : "released", index, name, reason
    );
#endif
}

int semDown (int my_semgid, unsigned int sindex)
{
    semdebug_logEvent(semdebug_channel, SEMDEBUG_DOWN, sindex, NULL);
    return semDown_raw(my_semgid, sindex);
}

int semUp (int my_semgid, unsigned int sindex)
{
    semdebug_logEvent(semdebug_channel, SEMDEBUG_UP, sindex, NULL);
    return semUp_raw(my_semgid, sindex);
}

int semDownOrExit(unsigned int index, const char *reason)
{
    int ret;

    semdebug_logEvent(semdebug_channel, SEMDEBUG_DOWN, index, reason);

    if ((ret = semDown_raw (semgid, index)) == -1) {
        perror ("semaphore down access failed (semDebug's semDownOrExit())");
        exit (EXIT_FAILURE);
    }

    return ret;
}

int semUpOrExit(unsigned int index, const char *reason)
{
    int ret;

    semdebug_logEvent(semdebug_channel, SEMDEBUG_UP, index, reason);

    if ((ret = semUp_raw (semgid, index)) == -1) {
        perror ("semaphore up access failed (semDebug's semDownOrExit())");
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
