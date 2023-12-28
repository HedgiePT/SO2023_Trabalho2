/**
 *  \file semSharedMemGroup.c (implementation file)
 *
 *  \brief Problem name: Restaurant
 *
 *  Synchronization based on semaphores and shared memory.
 *  Implementation with SVIPC.
 *
 *  Definition of the operations carried out by the groups:
 *     \li goToRestaurant
 *     \li checkInAtReception
 *     \li orderFood
 *     \li waitFood
 *     \li eat
 *     \li checkOutAtReception
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

// Extra semaphore functions written by the students.
#include "semDebug.h"

static void goToRestaurant (int id);
static void checkInAtReception (int id);
static void orderFood (int id);
static void waitFood (int id);
static void eat (int id);
static void checkOutAtReception (int id);

/**
 *  \brief Main program.
 *
 *  Its role is to generate the life cycle of one of intervening entities in the problem: the group.
 */
int main (int argc, char *argv[])
{
    int key;                                         /*access key to shared memory and semaphore set */
    char *tinp;                                                    /* numerical parameters test flag */
    int n;

    /* validation of command line parameters */
    if (argc != 5) { 
        freopen ("error_GR", "a", stderr);
        fprintf (stderr, "Number of parameters is incorrect!\n");
        return EXIT_FAILURE;
    }
    else {
     //  freopen (argv[4], "w", stderr);
       setbuf(stderr,NULL);
    }

    n = (unsigned int) strtol (argv[1], &tinp, 0);
    if ((*tinp != '\0') || (n >= MAXGROUPS )) { 
        fprintf (stderr, "Group process identification is wrong!\n");
        return EXIT_FAILURE;
    }
    strcpy (nFic, argv[2]);
    key = (unsigned int) strtol (argv[3], &tinp, 0);
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

    /* initialize random generator */
    srandom ((unsigned int) getpid ());                                                 

#ifdef SEMDEBUG
    semdebug_init(&sh->debug.groups[n]);
#endif

    /* simulation of the life cycle of the group */
    goToRestaurant(n);
    checkInAtReception(n);
    orderFood(n);
    waitFood(n);
    eat(n);
    checkOutAtReception(n);

    /* unmapping the shared region off the process address space */
    if (shmemDettach (sh) == -1) {
        perror ("error on unmapping the shared region off the process address space");
        return EXIT_FAILURE;;
    }

    return EXIT_SUCCESS;
}

/**
 *  \brief normal distribution generator with zero mean and stddev deviation. 
 *
 *  Generates random number according to normal distribution.
 * 
 *  \param stddev controls standard deviation of distribution
 */
static double normalRand(double stddev)
{
   int i;

   double r=0.0;
   for (i=0;i<12;i++) {
       r += random()/(RAND_MAX+1.0);
   }
   r -= 6.0;

   return r*stddev;
}

/**
 *  \brief group goes to restaurant 
 *
 *  The group takes its time to get to restaurant.
 *
 *  \param id group id
 */
static void goToRestaurant (int id)
{
    double startTime = sh->fSt.startTime[id] + normalRand(STARTDEV);
    
    if (startTime > 0.0) {
        usleep((unsigned int) startTime );
    }
}

/**
 *  \brief group eats
 *
 *  The group takes his time to eat a pleasant dinner.
 *
 *  \param id group id
 */
static void eat (int id)
{
    double eatTime = sh->fSt.eatTime[id] + normalRand(EATDEV);
    
    if (eatTime > 0.0) {
        usleep((unsigned int) eatTime );
    }
}

/**
 *  \brief group checks in at reception
 *
 *  Group should, as soon as receptionist is available, ask for a table,
 *  signaling receptionist of the request.  
 *  Group may have to wait for a table in this method.
 *  The internal state should be saved.
 *
 *  \param id group id
 *
 *  \return true if first group, false otherwise
 */
static void checkInAtReception(int id)
{
    semDownOrExit(sh->mutex, "pre-ATRECEPTION.");
        sh->fSt.st.groupStat[id] = ATRECEPTION;
        saveState(nFic, &sh->fSt);
    semUpOrExit(sh->mutex, "ATRECEPTION & state saved.");

    // Wait for receptionist to be ready by waiting on his semaphore.
    semDownOrExit(sh->receptionistRequestPossible, "before writing request for table.");

    // Now we can be sure we're the only one talking to the receptionist.
    sh->fSt.receptionistRequest = (request){ TABLEREQ, id };

    semUpOrExit(sh->receptionistReq, "requested a table to sit down.");
    semDownOrExit(sh->waitForTable[id], "waiting to sit down at table.");
}

/**
 *  \brief group orders food.
 *
 *  The group should update its state, request food to the waiter and 
 *  wait for the waiter to receive the request.
 *  
 *  The internal state should be saved.
 *
 *  \param id group id
 */
static void orderFood (int id)
{
    semDownOrExit (sh->mutex, "pre-FOOD_REQUEST.");
        sh->fSt.st.groupStat[id] = FOOD_REQUEST;
        saveState(nFic, &sh->fSt);
    semUpOrExit (sh->mutex, "FOOD_REQUEST & state saved.");
    
    // ----------------------------- //
    // TODO insert your code here

    semDownOrExit(sh->waiterRequestPossible, "waiting for waiter before ordering food.");
    sh->fSt.waiterRequest = (request){ FOODREQ, id };
    semUpOrExit (sh->waiterRequest, "finished writing food order.");

    int table = sh->fSt.assignedTable[id];
    semDownOrExit (sh->requestReceived[table], "waiting for waiter to receive our order.");
}

/**
 *  \brief group waits for food.
 *
 *  The group updates its state, and waits until food arrives. 
 *  It should also update state after food arrives.
 *  The internal state should be saved twice.
 *
 *  \param id group id
 */
static void waitFood (int id)
{
    semDownOrExit (sh->mutex, "pre-WAIT_FOR_FOOD");
        sh->fSt.st.groupStat[id] = WAIT_FOR_FOOD;
        saveState(nFic, &sh->fSt);
    semUpOrExit (sh->mutex, "WAIT_FOR_FOOD & state saved.");

    // TODO insert your code here
    int table = sh->fSt.assignedTable[id];

    semDownOrExit (sh->foodArrived[table], "waiting for our food to arrive.");

    semDownOrExit(sh->mutex, "pre-EAT");
        sh->fSt.st.groupStat[id] = EAT;
        saveState(nFic, &sh->fSt);
    semUpOrExit(sh->mutex, "EAT & state saved.");
}

/**
 *  \brief group check out at reception. 
 *
 *  The group, as soon as receptionist is available, updates its state and 
 *  sends a payment request to the receptionist.
 *  Group waits for receptionist to acknowledge payment. 
 *  Group should update its state to LEAVING, after acknowledge.
 *  The internal state should be saved twice.
 *
 *  \param id group id
 */
static void checkOutAtReception (int id)
{
    // Get table first; I have the suspicion the table info was getting swiped
    // from under our feet.
    int table = sh->fSt.assignedTable[id];

    semDownOrExit(sh->mutex, "pre-CHECKOUT");
        sh->fSt.st.groupStat[id] = CHECKOUT;
        saveState(nFic, &sh->fSt);
    semUpOrExit(sh->mutex, "CHECKOUT & state saved.");

    semDownOrExit(sh->receptionistRequestPossible, "before requesting bill.");

    // We can be sure we're the only one talking to the receptionist.
    sh->fSt.receptionistRequest = (request){ BILLREQ, id };
    semUpOrExit(sh->receptionistReq, "signaling bill requested.");
    semDownOrExit(sh->tableDone[table], "waiting for receptionist to acknowledge payment.");

    semDownOrExit(sh->mutex, "pre-LEAVING");
        sh->fSt.st.groupStat[id] = LEAVING;
        saveState(nFic, &sh->fSt);
    semUpOrExit(sh->mutex, "group left restaurant & state saved.");
}
