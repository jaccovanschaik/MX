#ifndef EVENTS_H
#define EVENTS_H

/*
 * types.h: Datatypes for MX.
 *
 * Copyright:	(c) 2014 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: types.h 438 2019-07-16 18:51:26Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <pthread.h>
#include <semaphore.h>

#include <libjvs/buffer.h>
#include <libjvs/list.h>
#include <libjvs/hash.h>
#include <libjvs/ml.h>
#include <libjvs/pa.h>
#include <libjvs/log.h>

#include "libmx.h"
#include "cmd.h"
#include "evt.h"

/*
 * The read- and write-end of a pipe.
 */
enum {
    RD = 0,
    WR = 1
};

/*
 * The size of an MX message header. Consists of a 32-bit message type id, a
 * 32-bit version and a 32-bit payload size field.
 */
#define HEADER_SIZE (3 * sizeof(uint32_t))

/*
 * Command to writer thread to write a message.
 */
typedef struct {
    uint32_t msg_type;                  /* Message type. */
    uint32_t version;                   /* Version. */
    uint32_t size;                      /* Payload size. */
    char *payload;                      /* Payload. */
} MX_WriteCommand;

/*
 * Command to timer thread to create a timer.
 */
typedef struct {
    uint32_t id;                        /* ID of timer. */
    double t;                           /* Time since epoch. */
                                        /* Callback and udata. */
    void (*handler)(MX *mx, uint32_t id, double t, void *udata);
    void *udata;
} MX_TimerCreateCommand;

/*
 * Command to timer thread to adjust a timer.
 */
typedef struct {
    uint32_t id;                        /* ID of timer to adjust. */
    double t;                           /* New time. */
} MX_TimerAdjustCommand;

/*
 * Command to timer thread to delete a timer.
 */
typedef struct {
    uint32_t id;                        /* ID of timer to delete. */
} MX_TimerDeleteCommand;

/*
 * Command to a thread.
 */
typedef struct {
    ListNode _node;
    MX_CommandType cmd_type;
    union {
        MX_WriteCommand write;
        MX_TimerCreateCommand timer_create;
        MX_TimerAdjustCommand timer_adjust;
        MX_TimerDeleteCommand timer_delete;
    } u;
} MX_Command;

/*
 * A command queue.
 */
typedef struct {
    List commands;
    pthread_mutex_t ok_to_access;
    sem_t ok_to_read;
} MX_Queue;

/*
 * MX timer data.
 */
typedef struct {
    ListNode _node;                     /* Make it listable. */
    uint32_t id;
    double t;                           /* Time since epoch. */
                                        /* Callback and udata. */
    void (*handler)(MX *mx, uint32_t id, double t, void *udata);
    void *udata;
} MX_Timer;

/*
 * MX await data.
 */
typedef struct {
    ListNode _node;                     /* Make it listable. */
    pthread_mutex_t mutex;              /* Mutex to wait for. */
    uint32_t msg_type;                  /* Type of message to wait for. */
    uint32_t version;                   /* Returned message version. */
    char *payload;                      /* Returned payload. */
    uint32_t size;                      /* Returned payload size. */
} MX_Await;

/*
 * An MX component.
 */
typedef struct {
    MX *mx;                             /* MX this component belongs to. */

    char *name;                         /* Name of the component. */
    char *host;                         /* Host on which it runs. */
    uint16_t port;                      /* Port on which it listens. */
    int fd;                             /* Connected to it on this fd. */

    MList subscriptions;                /* Its subscriptions. */

    Buffer incoming;                    /* Buffer for incoming data. */

    pthread_t reader_thread;            /* Reader thread id. */
    pthread_t writer_thread;            /* Writer thread id. */

    List awaits;                        /* List of awaits. */
    pthread_rwlock_t await_lock;        /* Lock to access await list. */

    MX_Queue writer_queue;              /* Command queue to writer thread. */
} MX_Component;

/*
 * A message type definition.
 */
typedef struct {
    uint32_t msg_type;                  /* Message type id. */
    char *msg_name;                     /* Message type name. */

    /* Callback on new subscribers. */
    void (*on_new_sub_callback)(MX *mx, int fd, uint32_t type, void *udata);
    void *on_new_sub_udata;

    /* Callback on ended subscribers. */
    void (*on_end_sub_callback)(MX *mx, int fd, uint32_t type, void *udata);
    void *on_end_sub_udata;

    MList subscriptions;                /* Subscriptions to this msg type. */
} MX_Message;

/*
 * Message subscription data.
 */
typedef struct {
    MListNode _node;                    /* Make it listable. */

    MX_Component *comp;                 /* Subscriber. */
    MX_Message *msg;                    /* Message type. */

    void (*handler)(MX *mx, int fd,
            uint32_t type, uint32_t version, char *payload, uint32_t size, void *udata);
    void *udata;
} MX_Subscription;

/*
 * New connection event data.
 */
typedef struct {
    int fd;                             /* FD of new connection. */
} MX_ConnectEvent;

/*
 * Closed connection event data.
 */
typedef struct {
    int fd;                             /* FD of closed connection. */
    char *whence;                       /* Where was the close detected? */
} MX_DisconnectEvent;

/*
 * Incoming message event data.
 */
typedef struct {
    int fd;                             /* FD where the message arrived. */
    uint32_t msg_type;                  /* Type of the message. */
    uint32_t version;                   /* Version of the message. */
    uint32_t size;                      /* Payload size. */
    char *payload;                      /* Payload. */
} MX_MessageEvent;

/*
 * Readable file descriptor event data.
 */
typedef struct {
    int fd;                             /* Readable file descriptor. */
} MX_ReadableEvent;

/*
 * Socket error event data.
 */
typedef struct {
    int fd;                             /* FD where the error occurred. */
    char *whence;                       /* Where it was detected. */
    int error;                          /* errno code. */
} MX_ErrorEvent;

/*
 * Event data.
 */
typedef struct {
    ListNode        _node;              /* Make it listable. */
    MX_EventType    evt_type;           /* Event type. */
    union {
        MX_ConnectEvent    conn;        /* Connect event data. */
        MX_DisconnectEvent disc;        /* Disconnect event data. */
        MX_MessageEvent    msg;         /* Message event data. */
        MX_Timer           timer;       /* Timer event data. */
        MX_ReadableEvent   read;        /* Readable event data. */
        MX_ErrorEvent      err;         /* Error event data. */
    } u;
} MX_Event;

/*
 * The MX struct.
 */
struct MX {
#if LOGGER == 1
    Logger *logger;
#endif

    char *mx_name;                      /* The MX name. */
    int listen_fd;                      /* File descriptor for listen port. */

    int event_pipe[2];                  /* Incoming event pipe. */

    pthread_t timer_thread;             /* Timer thread id. */
    pthread_t listener_thread;          /* Listener thread id. */

    List timers;                        /* List of timers. */

    MX_Queue timer_queue;               /* Command queue to timer thread. */

    MX_Component *master, *me;          /* The master component and myself. */

    PointerArray components;            /* Known components (indexed by FD). */

    HashTable message_by_type;          /* Message info hashed by type. */
    HashTable message_by_name;          /* Message info hashed by name. */

    uint32_t next_message_type;         /* Next message ID to be allocated. */

    int shutting_down;                  /* True if this MX is shutting down. */

    /* Callback on new components. */
    void (*on_new_comp_callback)(MX *mx, int fd, const char *name, void *udata);
    void *on_new_comp_udata;

    /* Callback on ended components. */
    void (*on_end_comp_callback)(MX *mx, int fd, const char *name, void *udata);
    void *on_end_comp_udata;

    /* Callback on registered messages. */
    void (*on_register_callback)(MX *mx, uint32_t type, const char *name, void *udata);
    void *on_register_udata;
};

#endif
