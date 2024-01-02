/**
 *  \file semSharedReceptionist.c (implementation file)
 *
 *  \brief Problem name: Restaurant
 *
 *  Synchronization based on semaphores and shared memory.
 *  Implementation with SVIPC.
 *
 *  Definition of the operations carried out by the receptionist:
 *     \li waitForGroup
 *     \li provideTableOrWaitingRoom
 *     \li receivePayment
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

#include "semDebug.h"

/* constants for groupRecord */
#define TOARRIVE 0
#define WAIT     1
#define ATTABLE  2
#define DONE     3

/** \brief receptioninst view on each group evolution (useful to decide table binding) */
static int groupRecord[MAXGROUPS];

/** \brief more useful view of groupRecord. Student-made. **/
struct record {
    unsigned int status;
    unsigned char n_occupied, next_table;
    int list_begin, list_end;
    unsigned char waitlist[(MAXGROUPS-2) * sizeof(int) / sizeof(char)];
} *rec = (struct record *)groupRecord;

static int WAITLIST_LENGTH = sizeof(rec->waitlist) / sizeof(rec->waitlist[0]);
static bool is_table_occupied(int t) { return rec->status & (1 << t); }
static void set_table_occupied(int t, bool occupied)
{
    if (occupied) {
        rec->status |= (1 << t);
        rec->n_occupied++;
        rec->next_table = (t+1) % NUMTABLES;
    } else {
        rec->status &= ~(1 << t);
        rec->n_occupied--;
    }
}


/** \brief receptionist waits for next request */
static request waitForGroup ();

/** \brief receptionist waits for next request */
static void provideTableOrWaitingRoom (int n);

/** \brief receptionist receives payment */
static void receivePayment (int n);



/**
 *  \brief Main program.
 *
 *  Its role is to generate the life cycle of one of intervening entities in the problem: the receptionist.
 */
int main (int argc, char *argv[])
{
    int key;                                            /*access key to shared memory and semaphore set */
    char *tinp;                                                       /* numerical parameters test flag */

    /* validation of command line parameters */
    if (argc != 4) { 
        freopen ("error_RT", "a", stderr);
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
    semdebug_init(&sh->debug.receptionist);
#endif

    /* initialize random generator */
    srandom ((unsigned int) getpid ());              

    /* initialize internal receptionist memory */
    int g;
    for (g=0; g < sh->fSt.nGroups; g++) {
       groupRecord[g] = TOARRIVE;
    }

    /* simulation of the life cycle of the receptionist */
    int nReq=0;
    request req;
    while( nReq < sh->fSt.nGroups*2 ) {
        req = waitForGroup();
        switch(req.reqType) {
            case TABLEREQ:
                   provideTableOrWaitingRoom(req.reqGroup); //TODO param should be groupid
                   break;
            case BILLREQ:
                   receivePayment(req.reqGroup);
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
 *  \brief decides table to occupy for group n or if it must wait.
 *
 *  Checks current state of tables and groups in order to decide table or wait.
 *
 *  \return table id or -1 (in case of wait decision)
 */
static int decideTableOrWait(int n)
{
    if (rec->n_occupied == NUMTABLES)
        return -1;

    for (int i = 0; i < NUMTABLES; i++)
    {
        int index = (i + rec->next_table) % NUMTABLES;
        bool occupied = is_table_occupied(index);

        if (occupied)
            continue;
        else
            return index;
    }

    return -1;
}

/**
 *  \brief called when a table gets vacant and there are waiting groups 
 *         to decide which group (if any) should occupy it.
 *
 *  Checks current state of tables and groups in order to decide group.
 *
 *  \return group id or -1 (in case of wait decision)
 */
static int decideNextGroup()
{
    int g = -1;
    
    if (sh->fSt.groupsWaiting) {
        g = rec->waitlist[rec->list_begin];
        rec->list_begin = (rec->list_begin + 1) % WAITLIST_LENGTH;
        sh->fSt.groupsWaiting--;
    }
    
    return g;
}

/**
 *  \brief receptionist waits for next request 
 *
 *  Receptionist updates state and waits for request from group, then reads request,
 *  and signals availability for new request.
 *  The internal state should be saved.
 *
 *  \return request submitted by group
 */
static request waitForGroup()
{
    request ret; 

    semDownOrExit(sh->mutex, NULL);
        // No status code provided for "waiting".
        sh->fSt.st.receptionistStat = 0;
        saveState(nFic, &(sh->fSt));
    semUpOrExit(sh->mutex, "state changed to 0 (waiting).");

    semDownOrExit(sh->receptionistReq, "waiting for requests.");
        ret = sh->fSt.receptionistRequest;
    semUpOrExit(sh->receptionistRequestPossible, "finished reading request.");

    return ret;
}

/**
 *  \brief receptionist decides if group should occupy table or wait
 *
 *  Receptionist updates state and then decides if group occupies table
 *  or waits. Shared (and internal) memory may need to be updated.
 *  If group occupies table, it must be informed that it may proceed. 
 *  The internal state should be saved.
 *
 */
static void provideTableOrWaitingRoom (int n)
{
    semDownOrExit(sh->mutex, NULL);
        sh->fSt.st.receptionistStat = ASSIGNTABLE;
        saveState(nFic, &(sh->fSt));
    semUpOrExit(sh->mutex, "new state: ASSIGNTABLE.");
    
    int table = decideTableOrWait(n);
    
    if (table > -1) {
        set_table_occupied(table, true);
        sh->fSt.assignedTable[n] = table;
        semUpOrExit(sh->waitForTable[n], "assigned table to group.");
    } else {
        rec->waitlist[rec->list_end] = n;
        rec->list_end = (rec->list_end + 1) % WAITLIST_LENGTH;
        sh->fSt.groupsWaiting++;
    }
}

/**
 *  \brief receptionist receives payment 
 *
 *  Receptionist updates its state and receives payment.
 *  If there are waiting groups, receptionist should check if table that just became
 *  vacant should be occupied. Shared (and internal) memory should be updated.
 *  The internal state should be saved.
 *
 */

static void receivePayment (int n)
{
    semDownOrExit(sh->mutex, NULL);
        sh->fSt.st.receptionistStat = RECVPAY;
        saveState(nFic, &sh->fSt);
    semUpOrExit(sh->mutex, "new state: RECVPAY");

    int group = n;
    int table = sh->fSt.assignedTable[group];

    if (table < 0) {
        semDownOrExit(sh->mutex, "!!! BUG: Table not found!");
        sleep(-1);
    }

    sh->fSt.assignedTable[group] = -1;
    set_table_occupied(table, false);
    semUpOrExit(sh->tableDone[table], "Signalling payment received");

    if ((group = decideNextGroup()) > -1) {
        provideTableOrWaitingRoom(group);
    }
}

