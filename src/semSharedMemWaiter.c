/**
 *  \file semSharedWaiter.c (implementation file)
 *
 *  \brief Problem name: Restaurant
 *
 *  Synchronization based on semaphores and shared memory.
 *  Implementation with SVIPC.
 *
 *  Definition of the operations carried out by the waiter:
 *     \li waitForClientOrChef
 *     \li informChef
 *     \li takeFoodToTable
 *
 *  \author Nuno Lau - December 2023
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "probConst.h"
#include "probDataStruct.h"
#include "logging.h"
#include "sharedDataSync.h"
#include "semaphore.h"
#include "sharedMemory.h"

/** \brief logging file name */
static char nFic[51];

/** \brief shared memory block access identifier */
static int shmid;

/** \brief semaphore set access identifier */
static int semgid;

/** \brief pointer to shared memory region */
static SHARED_DATA *sh;

// Made by the students.
#include "semDebug.h"

/** \brief waiter waits for next request */
static request waitForClientOrChef ();

/** \brief waiter takes food order to chef */
static void informChef(int group);

/** \brief waiter takes food to table */
static void takeFoodToTable (int group);




/**
 *  \brief Main program.
 *
 *  Its role is to generate the life cycle of one of intervening entities in the problem: the waiter.
 */
int main (int argc, char *argv[])
{
    int key;                                            /*access key to shared memory and semaphore set */
    char *tinp;                                                       /* numerical parameters test flag */

    /* validation of command line parameters */
    if (argc != 4) { 
        freopen ("error_WT", "a", stderr);
        fprintf (stderr, "Number of parameters is incorrect!\n");
        return EXIT_FAILURE;
    }
    else { 
        freopen (argv[3], "w", stderr);
        setbuf(stderr,NULL);
    }

    strcpy (nFic, argv[1]);
    key = (unsigned int) strtol (argv[2], &tinp, 0);
    if (*tinp != '\0') {
        fprintf (stderr, "Error on the access key communication!\n");
        return EXIT_FAILURE;
    }

    /* connection to the semaphore set and the shared memory region and mapping the shared region onto the
       process address space */
    if ((semgid = semConnect (key)) == -1) { 
        perror ("error on connecting to the semaphore set");
        return EXIT_FAILURE;
    }
    if ((shmid = shmemConnect (key)) == -1) { 
        perror ("error on connecting to the shared memory region");
        return EXIT_FAILURE;
    }
    if (shmemAttach (shmid, (void **) &sh) == -1) { 
        perror ("error on mapping the shared region on the process address space");
        return EXIT_FAILURE;
    }

#ifdef SEMDEBUG
    semdebug_init(&sh->debug.waiter);
#endif

    /* initialize random generator */
    srandom ((unsigned int) getpid ());              

    /* simulation of the life cycle of the waiter */
    int nReq=0;
    request req;
    while( nReq < sh->fSt.nGroups*2 ) {
        req = waitForClientOrChef();
        switch(req.reqType) {
            case FOODREQ:
                   informChef(req.reqGroup);
                   break;
            case FOODREADY:
                   takeFoodToTable(req.reqGroup);
                   break;
        }
        nReq++;
    }

    /* unmapping the shared region off the process address space */
    if (shmemDettach (sh) == -1) {
        perror ("error on unmapping the shared region off the process address space");
        return EXIT_FAILURE;;
    }

    return EXIT_SUCCESS;
}

/**
 *  \brief waiter waits for next request 
 *
 *  Waiter updates state and waits for request from group or from chef, then reads request.
 *  The waiter should signal that new requests are possible.
 *  The internal state should be saved.
 *
 *  \return request submitted by group or chef
 */
static request waitForClientOrChef()
{
    // A request queue to hold onto while chef cooks.
    static request queue[NUMTABLES];
    static size_t qlength = 0, qread_next = 0, qwrite_next = 0;

    // If chef is already tasked with an order.
    const bool chef_is_busy = sh->fSt.foodOrder;

    // Return value.
    request outgoing = { -1, -1 };


    while (outgoing.reqType == -1) {
        if (qlength > 0 && !chef_is_busy) {
            outgoing = queue[qread_next];
            qread_next = (qread_next + 1) % NUMTABLES;
            qlength--;
        } else {
            if (sh->fSt.st.waiterStat != WAIT_FOR_REQUEST) {
                semDownOrExit(sh->mutex, "pre-WAIT_FOR_REQUEST");
                    sh->fSt.st.waiterStat = WAIT_FOR_REQUEST;
                    saveState(nFic, &(sh->fSt));
                semUpOrExit (sh->mutex, "WAIT_FOR_REQUEST & state saved.");
            }

            // Wait for incoming request.
            semDownOrExit(sh->waiterRequest, "waiting for incoming requests");
            const request incoming = sh->fSt.waiterRequest;
            semUpOrExit(sh->waiterRequestPossible,
                        "signalling new requests are possible");

            if (incoming.reqType == FOODREQ) {
                queue[qwrite_next] = incoming;
                qwrite_next = (qwrite_next + 1) % NUMTABLES;
                qlength++;
            } else if (incoming.reqType == FOODREADY) {
                outgoing = incoming;
            } else {
                semDownOrExit(sh->mutex, "!!! BUG: Wrong request.");
                sleep(-1);
            }
        }
    }

    return outgoing;
}

/**
 *  \brief waiter takes food order to chef 
 *
 *  Waiter updates state and then takes food request to chef.
 *  Waiter should inform group that request is received.
 *  Waiter should wait for chef receiving request.
 *  The internal state should be saved.
 *
 */
static void informChef (int n)
{
    semDownOrExit(sh->mutex, "pre-INFORM_CHEF");
        sh->fSt.foodGroup = n;
        sh->fSt.foodOrder = true;
        sh->fSt.st.waiterStat = INFORM_CHEF;
        saveState(nFic, &(sh->fSt));
    semUpOrExit (sh->mutex, "INFORM_CHEF & state saved.");

    semUpOrExit(sh->waitOrder, "we have an order for chef");

    semDownOrExit(sh->orderReceived, "waiter waits for chef to receive request");
    int table = sh->fSt.assignedTable[n];
    semUpOrExit(sh->requestReceived[table], "waiter informs that request was received");
}

/**
 *  \brief waiter takes food to table 
 *
 *  Waiter updates its state and takes food to table, allowing the meal to start.
 *  Group must be informed that food is available.
 *  The internal state should be saved.
 *
 */



static void takeFoodToTable (int n)
{
    semDownOrExit (sh->mutex, "pre-TAKE_TO_TABLE");
        sh->fSt.st.waiterStat = TAKE_TO_TABLE;
        saveState(nFic, &(sh->fSt));
        int table = sh->fSt.assignedTable[n];
    semUpOrExit (sh->mutex, "TAKE_TO_TABLE & state saved.");

    sh->fSt.foodOrder = false;

    semUpOrExit(sh->foodArrived[table], "food arrives at the table");
}

