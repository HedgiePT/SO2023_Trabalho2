#pragma once

// Developed by the students.

#ifdef SEMDEBUG
#include <dirent.h>
#include <sys/sem.h>

#define SEMDEBUG_MAX_EVENTS 150
#define SEMDEBUG_MAX_REASON 100

struct semdebug_event
{
    enum SEMDEBUG_ACTION
    {
        SEMDEBUG_UNDEFINED = 0,
        SEMDEBUG_DOWN,
        SEMDEBUG_UP
    } action;
    unsigned int index;
    char reason[SEMDEBUG_MAX_REASON];
};

struct semdebug_buff
{
    pid_t pid;
    size_t next_slot;
    struct semdebug_event events[SEMDEBUG_MAX_EVENTS];
};

struct semdebug
{
    struct semdebug_buff chef, waiter, receptionist, groups[MAXGROUPS];
};

void semdebug_writeEvToBuffer(struct semdebug_buff *buffer, enum SEMDEBUG_ACTION action, unsigned int index, const char *reason)
{
    if (!buffer)
        return;

    struct semdebug_event *out = &buffer->events[buffer->next_slot];
    out->action = action;
    out->index = index;
    strlcpy(out->reason, reason, SEMDEBUG_MAX_REASON);

    buffer->next_slot = (buffer->next_slot + 1) % SEMDEBUG_MAX_EVENTS;
}

size_t semdebug_getAllEvSorted(const struct semdebug_buff *in, struct semdebug_event *out)
{
    if (!(in && out))
        return false;

    struct semdebug_event *outslot = out;
    size_t nWritten = 0;

    for (size_t i = 0; i < SEMDEBUG_MAX_EVENTS; i++)
    {
        size_t current_slot = (in->next_slot + i) % SEMDEBUG_MAX_EVENTS;
        const struct semdebug_event *ev = &in->events[current_slot];

        if (ev->action == SEMDEBUG_UNDEFINED)
            continue;

        *(outslot++) = *ev;
        nWritten++;
    }

    return nWritten;
}

// -*-*- DIAGNOSTICS -*-*-

struct semdebug_diag_proc
{
    pid_t pid;
    int stage;
    int waiting_on;
    bool exited;
    size_t n_events;
    struct semdebug_event events[SEMDEBUG_MAX_EVENTS];
    struct semdebug_event *last_event;
};

struct semdebug_diag_procset
{
    struct semdebug_diag_proc ch, wt, rc, gr[MAXGROUPS];
};

int get_semaphore_name(const FULL_STAT *fd, unsigned int index,
                       char *out, size_t n)
{
    int nWritten = 0;
    const char *s = NULL;
    
    const int WAITFORTABLE = 8;
    const int FOODARRIVED = WAITFORTABLE + fd->nGroups;
    const int REQUESTRECEIVED = FOODARRIVED + NUMTABLES;
    const int TABLEDONE = REQUESTRECEIVED + NUMTABLES;

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

static const char *get_group_stage_label(int stage)
{
    switch (stage)
    {
        case 1: return "GOTOREST";
        case 2: return "ATRECEPTION";
        case 3: return "FOOD_REQUEST";
        case 4: return "WAIT_FOR_FOOD";
        case 5: return "EAT";
        case 6: return "CHECKOUT";
        case 7: return "LEAVING";
        default: return "";
    }
}

static const char *get_chef_stage_label(int stage)
{
        switch (stage)
        {
            case 0: return "WAIT_FOR_ORDER";
            case 1: return "COOK";
            case 2: return "REST";
            default: return "";
        }
}

static const char *get_waiter_stage_label(int stage)
{
    switch (stage)
    {
        case 0: return "WAIT_FOR_REQST";
        case 1: return "INFORM_CHEF";
        case 2: return "TAKE_TO_TABLE";
        default: return "";
    }
}

static const char *get_receptionist_stage_label(int stage)
{
    switch (stage)
    {
        case 1: return "ASSIGNTABLE";
        case 2: return "RECVPAY";
        default: return "";
    }
}

static const char *get_request_label(int request)
{
    switch (request)
    {
        case 1: return "TABLEREQ";
        case 2: return "BILLREQ";
        case 3: return "FOODREQ";
        case 4: return "FOODREADY";
        default: return "";
    }
}

#define MYMIN(a, b) ((a) < (b) ? (a) : (b))

bool semdebug_getProcDiagnostics(const struct semdebug *sd, const FULL_STAT *fd,
                        int semgid, struct semdebug_diag_procset *out)
{
    char buf[50];
    
    if (!(sd && fd && out))
        return false;
    
    out->ch.pid = sd->chef.pid;
    out->ch.stage = fd->st.chefStat;
    snprintf(buf, sizeof(buf)/sizeof(buf[0]), "/proc/%d", out->ch.pid);
    out->ch.exited = (bool)(!opendir(buf));
    out->ch.n_events = semdebug_getAllEvSorted(&sd->chef, out->ch.events);
    out->ch.last_event = out->ch.n_events ?
        &out->ch.events[out->ch.n_events - 1] : NULL;
    
    out->wt.pid = sd->waiter.pid;
    out->wt.stage = fd->st.waiterStat;
    snprintf(buf, sizeof(buf)/sizeof(buf[0]), "/proc/%d", out->wt.pid);
    out->wt.exited = (bool)(!opendir(buf));
    out->wt.n_events = semdebug_getAllEvSorted(&sd->waiter, out->wt.events);
    out->wt.last_event = out->wt.n_events ?
        &out->wt.events[out->wt.n_events - 1] : NULL;
    
    out->rc.pid = sd->receptionist.pid;
    out->rc.stage = fd->st.receptionistStat;
    snprintf(buf, sizeof(buf)/sizeof(buf[0]), "/proc/%d", out->rc.pid);
    out->rc.exited = (bool)(!opendir(buf));
    out->rc.n_events = semdebug_getAllEvSorted(&sd->receptionist, out->rc.events);
    out->rc.last_event = out->rc.n_events ?
        &out->rc.events[out->rc.n_events - 1] : NULL;
    
    for (size_t i = 0; i < fd->nGroups; i++)
    {
        struct semdebug_diag_proc *gr = &out->gr[i];
        const struct semdebug_buff *grin = &sd->groups[i];
        
        gr->pid = grin->pid;
        gr->stage = fd->st.groupStat[i];
        snprintf(buf, sizeof(buf)/sizeof(buf[0]), "/proc/%d", gr->pid);
        gr->exited = (bool)(!opendir(buf));
        gr->n_events = semdebug_getAllEvSorted(grin, gr->events);
        gr->last_event = gr->n_events ? &gr->events[gr->n_events - 1] : NULL;
    }
    
    
    
    return true;
}

void semdebug_print_deadlock_logs(const struct semdebug_event *ev,
                                  const struct semdebug_event *last,
                                  const FULL_STAT *fd)
{
    const char format_details_divider[] = "\
┃ ║ ──────────────────── last %2d semaphore operations ────────────────────── ║ ┃\n\
┃ ║                          (most recent last)                              ║ ┃\n\
┃ ║                                                                          ║ ┃\n";
    const char format_event_log[] = "\
┃ ║ %2u: %s %-66s ║ ┃\n";

    const char no_events_captured[] = "\
┃ ║ (no events captured)                                                     ║ ┃\n";
    
    fprintf(stderr, format_details_divider, SEMDEBUG_MAX_EVENTS);

    if (last)
    {
        unsigned int count = 1;
        
        while (ev <= last)
        {
            char emoji[5];
            
            switch (ev->action)
            {
                case SEMDEBUG_UP:
                    strlcpy(emoji, "▲", 5);
                    break;
                case SEMDEBUG_DOWN:
                    strlcpy(emoji, "▼", 5);
                    break;
                default:
                    strlcpy(emoji, "?", 5);
            }
            
            const int buffer_size = 200;
            char buffer[buffer_size];
            get_semaphore_name(fd, ev->index, buffer, buffer_size);
            
            strlcat(buffer, ": ", buffer_size);
            if (strlcat(buffer, ev->reason, buffer_size) > 65)
            {
                buffer[65] = '\0';
                strlcat(buffer, "…", buffer_size);
            }
            
            fprintf(stderr, format_event_log, count, emoji, buffer);
            
            ev++;
            count++;
        }
    }
    else
    {
        fprintf(stderr, no_events_captured);
    }
}

void semdebug_print_deadlock(struct semdebug *sd, const FULL_STAT *fd, int semgid)
{
    const char banner[] = "\
============================== DEADLOCK DETECTED! =============================\n\n";
    const char header_summary[] = "\
┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━[ Summary ]━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓\n\
┃                                                                              ┃\n\
┃  TIP: Asterisk (*) marks processes that are still open.                      ┃\n\
┃       Check if any open process is waiting for one that's already finished.  ┃\n\
┃                                                                              ┃\n";
    const char header_3summary[] = "\
┃ ╔═══ Chef / Waiter / Receptionist ═════════════════════════════════════════╗ ┃\n\
┃ ║     │ Stage            │ LastSem │ Last reason                           ║ ┃\n\
┃ ║     ╆━━━━━━━━━━━━━━━━━━┿━━━━━━━━━┿━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━║ ┃\n";
    const char format_3summary[] = "┃ ║ %c%2s ┃ %d %-14s │ %4s %2d │ %-37s ║ ┃\n";
    const char footer_3summary[] = "\
┃ ╚══════════════════════════════════════════════════════════════════════════╝ ┃\n";
    const char *const footer_grsummary = footer_3summary;
    const char toplevel_spacer[] = "\
┃                                                                              ┃\n";

    fprintf(stderr, banner);
    
    struct semdebug_diag_procset procset;
    semdebug_getProcDiagnostics(sd, fd, semgid, &procset);
    
    fprintf(stderr, header_summary);
    fprintf(stderr, header_3summary);
    
    bool has_events;
    
    // Printing CH stuff.
    has_events = procset.ch.n_events;
    
    fprintf(stderr, format_3summary,
            (!procset.ch.exited) ? '*' : ' ',
            "CH",
            procset.ch.stage,
            get_chef_stage_label(procset.ch.stage),
            has_events ? (procset.ch.last_event->action == SEMDEBUG_DOWN ?
                    "down" : "up")
                : "?",
            has_events ? (procset.ch.last_event->index) : 0,
            has_events ? procset.ch.last_event->reason : "?"
    );
    
    // Now print WT stuff.
    has_events = procset.wt.n_events;
    
    fprintf(stderr, format_3summary,
            (!procset.wt.exited) ? '*' : ' ',
            "WT",
            procset.wt.stage,
            get_waiter_stage_label(procset.wt.stage),
            has_events ? (procset.wt.last_event->action == SEMDEBUG_DOWN ?
                    "down" : "up")
                : "?",
            has_events ? (procset.wt.last_event->index) : 0,
            has_events ? procset.wt.last_event->reason : "?"
    );
    
    // Now print RC stuff.
    has_events = procset.rc.n_events;
    
    fprintf(stderr, format_3summary,
            (!procset.rc.exited) ? '*' : ' ',
            "RC",
            procset.rc.stage,
            get_receptionist_stage_label(procset.rc.stage),
            has_events ?
                (procset.rc.last_event->action == SEMDEBUG_DOWN ?
                    "down" : "up")
                : "?",
            has_events ? (procset.rc.last_event->index) : 0,
            has_events ? procset.rc.last_event->reason : "?"
    );

    fprintf(stderr, footer_3summary);
    fprintf(stderr, toplevel_spacer);
    
    const char header_grsummary[] = "\
┃ ╔═══ Groups ═══════════════════════════════════════════════════════════════╗ ┃\n\
┃ ║     │ Stage           │ LastSem │ Last reason                   │ Table  ║ ┃\n\
┃ ║     ╆━━━━━━━━━━━━━━━━━┿━━━━━━━━━┿━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┿━━━━━━━ ║ ┃\n";
    
    const char format_grsummary[] =
    "┃ ║ %c%2d ┃ %d %-13s │ %4s %2d │ %-29s │ %3d    ║ ┃\n";

    fprintf(stderr, header_grsummary);

    for (int i = 0; i < fd->nGroups; i++)
    {
        const struct semdebug_diag_proc *g = &procset.gr[i];
        const struct semdebug_event *le = g->last_event;
        char trimmed_reason[40];
        
        has_events = g->n_events;
        if (has_events)
            if (strlcpy(trimmed_reason, le->reason, 40) > 28)
            {
                trimmed_reason[28] = '\0';
                strlcat(trimmed_reason, "…", 40);
            }
        
        fprintf(stderr, format_grsummary,
                (!g->exited) ? '*' : ' ',
                i,
                g->stage,
                get_group_stage_label(g->stage),
                has_events ? (le->action == SEMDEBUG_DOWN ?
                    "down" : "up")
                : "?",
                has_events ? le->index : 0,
                has_events ? trimmed_reason : "?",
                fd->assignedTable[i]
        );
    }
    
    fprintf(stderr, footer_grsummary);
    fprintf(stderr, toplevel_spacer);

     const char format_summary_extrainfo[] = "\
┃   ╔═══ Waiter request ═══╗  ╔═══ Other info ═══════╗                         ┃\n\
┃   ║  type: %-9s (%d) ║  ║        nGroups: %d    ║                         ┃\n\
┃   ║ %5s: %d             ║  ║  groupsWaiting: %d    ║                         ┃\n\
┃   ╚══════════════════════╝  ║      foodOrder: %d    ║                         ┃\n\
┃   ╔═══ Recep. request ═══╗  ║      foodGroup: %d    ║                         ┃\n\
┃   ║  type: %-9s (%d) ║  ╚══════════════════════╝                         ┃\n\
┃   ║ group: %d             ║                                                   ┃\n\
┃   ╚══════════════════════╝                                                   ┃\n";

     fprintf(stderr, format_summary_extrainfo,
             get_request_label(fd->waiterRequest.reqType),  // Waiter request
             fd->waiterRequest.reqType,                     // Waiter request
             fd->nGroups,                                   // nGroups
             fd->waiterRequest.reqType == TABLEREQ ? "group" : "table",
             fd->waiterRequest.reqGroup,
             fd->groupsWaiting,
             fd->foodOrder,
             fd->foodGroup,
             get_request_label(fd->receptionistRequest.reqType),
             fd->receptionistRequest.reqType,
             fd->receptionistRequest.reqGroup
     );
     
    const char toplevel_footer[] = "┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\n";
    
    fprintf(stderr, toplevel_footer);
    fprintf(stderr, "\n\n");
    
    const char header_detailed_log[] = "\
┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━[ Detailed log ]━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓\n\
┃                                                                              ┃\n";
    
    const char format_header_details_chef[] = "\
┃ ╔═══ Chef ═════════════════════════════════════════════════════════════════╗ ┃\n\
┃ ║ stage: %d %-17s    PID: %-9d    process status: %-8s ║ ┃\n\
┃ ║                                                                          ║ ┃\n";
    
    fprintf(stderr, header_detailed_log);
    fprintf(stderr, format_header_details_chef,
            procset.ch.stage, get_chef_stage_label(procset.ch.stage),
            procset.ch.pid, procset.ch.exited ? "quit" : "present"
    );
    
    semdebug_print_deadlock_logs(procset.ch.events, procset.ch.last_event, fd);
    fprintf(stderr, footer_3summary);
    
    const char format_header_details_waiter[] = "\
┃ ╔═══ Waiter ═══════════════════════════════════════════════════════════════╗ ┃\n\
┃ ║ stage: %d %-17s    PID: %-9d    process status: %-8s ║ ┃\n\
┃ ║ reqtype: %d %-11s        reqGroup: %d                                ║ ┃\n\
┃ ║                                                                          ║ ┃\n";
    
    fprintf(stderr, format_header_details_waiter,
            procset.wt.stage, get_waiter_stage_label(procset.wt.stage),
            procset.wt.pid, procset.wt.exited ? "quit" : "present",
            fd->waiterRequest.reqType,
            get_request_label(fd->waiterRequest.reqType),
            fd->waiterRequest.reqGroup);
    
    semdebug_print_deadlock_logs(procset.wt.events, procset.wt.last_event, fd);
    fprintf(stderr, footer_3summary);

    const char format_header_details_receptionist[] = "\
┃ ╔═══ Receptionist ═════════════════════════════════════════════════════════╗ ┃\n\
┃ ║ stage: %d %-17s    PID: %-9d    process status: %-8s ║ ┃\n\
┃ ║                                                                          ║ ┃\n";

    fprintf(stderr, format_header_details_receptionist,
            procset.rc.stage, get_receptionist_stage_label(procset.rc.stage),
            procset.rc.pid, procset.rc.exited ? "quit" : "present"
    );
    
    semdebug_print_deadlock_logs(procset.rc.events, procset.rc.last_event, fd);
    fprintf(stderr, footer_3summary);

    const char format_header_details_group[] = "\
┃ ╔═══ Group %02d ═════════════════════════════════════════════════════════════╗ ┃\n\
┃ ║ stage: %d %-17s    PID: %-9d    process status: %-8s ║ ┃\n\
┃ ║ table: %2d                                                                ║ ┃\n";

    for (int i = 0; i < fd->nGroups; i++)
    {
        const struct semdebug_diag_proc *g = &procset.gr[i];
        
        fprintf(stderr, format_header_details_group,
                i, g->stage, get_group_stage_label(g->stage),
                g->pid, g->exited ? "quit" : "present", fd->assignedTable[i]
        );
        
        semdebug_print_deadlock_logs(g->events, g->last_event, fd);
        fprintf(stderr, footer_3summary);
    }
    
    fprintf(stderr, toplevel_footer);
    
    fprintf(stderr, "Press ENTER to exit.\n");
    scanf(".");
}

#endif //SEMDEBUG
