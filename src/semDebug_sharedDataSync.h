#pragma once

// Developed by the students.

#ifdef SEMDEBUG

#define SEMDEBUG_MAX_EVENTS 10
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

    size_t nWritten = 0;

    for (size_t i = 0; i < SEMDEBUG_MAX_EVENTS; i++)
    {
        size_t current_slot = (in->next_slot + i) % SEMDEBUG_MAX_EVENTS;
        const struct semdebug_event *ev = &in->events[current_slot];

        if (ev->action == SEMDEBUG_UNDEFINED)
            continue;

        out[i] = *ev;
        nWritten++;
    }

    return nWritten;
}

#endif //SEMDEBUG
