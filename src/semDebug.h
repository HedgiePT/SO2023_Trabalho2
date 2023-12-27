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

int get_semaphore_name(unsigned int index, char *out, size_t n)
{
    int nWritten = 0;
    const char *s = NULL;

    switch (index)
    {
        case 0: s = "NULL (unused)"; break;
        case 1: s = "mutex"; break;
        case 2: s = "receptionistReq"; break;
        case 3: s = "receptionistRequestPossible"; break;
        case 4: s = "waiterRequest"; break;
        case 5: s = "waiterRequestPossible"; break;
        case 6: s = "waitOrder"; break;
        case 7: s = "orderReceived"; break;
        default:
            if (index >= WAITFORTABLE && index < FOODARRIVED) {
                nWritten = snprintf(out, n, "waitForTable (group %d)",
                         index - WAITFORTABLE
                );
            } else if (index >= FOODARRIVED && index < REQUESTRECEIVED) {
                nWritten = snprintf(out, n, "foodArrived (table %d)",
                         index - FOODARRIVED
                );
            } else if (index >= REQUESTRECEIVED && index < TABLEDONE) {
                nWritten = snprintf(out, n, "requestReceived (table %d)",
                         index - REQUESTRECEIVED
                );
            } else if (index >= TABLEDONE && index < TABLEDONE+NUMTABLES) {
                nWritten = snprintf(out, n, "tableDone (table %d)",
                         index - TABLEDONE
                );
            } else {
                s = "(UNKNOWN SEMAPHORE)";
            }
    }

    if (s)
        nWritten = strlcpy(out, s, n);

    return nWritten;
}

int semDownOrExit(unsigned int index, const char *reason)
{
    int ret;
    char name[100];

    if (!reason)
        reason = "(no reason provided)";

    get_semaphore_name(index, name, 100);

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

    get_semaphore_name(index, name, 100);
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
