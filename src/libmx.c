/*
 * libmx.c: Message Exchange library.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: libmx.c 122 2013-10-16 13:15:03Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>

#include <libjvs/ns.h>
#include <libjvs/ns-types.h>
#include <libjvs/ml.h>
#include <libjvs/pa.h>
#include <libjvs/net.h>
#include <libjvs/tcp.h>
#include <libjvs/hash.h>
#include <libjvs/debug.h>
#include <libjvs/utils.h>
#include <libjvs/buffer.h>

#include "libmx.h"
#include "private.h"

/*
 * The various event types that we can handle.
 */
typedef enum {
    MX_ET_NONE,
    MX_ET_DATA,                         /* Data on an mxOnFile file descriptor. */
    MX_ET_MESSAGE,                      /* Incoming message on a messaging socket. */
    MX_ET_TIMER,                        /* mxOnTime timer going off. */
    MX_ET_AWAIT,                        /* mxAwait timeout. */
    MX_ET_ERROR,                        /* Error while awaiting/reading messages. */
} MX_EventType;

/*
 * A data event (incoming data on a file descriptor given in an mxOnFile call).
 */
typedef struct {
    int fd;                             /* The file descriptor that has data. */
} MX_DataEvent;

/*
 * A message event (incoming message on a connected messaging socket).
 */
typedef struct {
    int fd;
    MX_Type type;
    MX_Version version;
    MX_Size size;
    char *payload;
} MX_MessageEvent;

/*
 * An error event (error occurred while waiting for or reading a file descriptor).
 */
typedef struct {
    int fd;                             /* File descriptor where the error occurred. */
    int error;                          /* errno error code. */
} MX_ErrorEvent;

/*
 * A connection event (new connection on an mxListen socket).
 */
typedef struct {
    int fd;                             /* Listen socket file descriptor. */
} MX_ConnEvent;

/*
 * A disconnect event (connection broken/end-of-file).
 */
typedef struct {
    int fd;                             /* File descriptor of broken connection. */
} MX_DiscEvent;

/*
 * This is the event "superclass".
 */
typedef struct {
    ListNode _node;
    MX_EventType event_type;            /* Type of the event. */
    union {
        MX_DataEvent data;
        MX_MessageEvent msg;
        MX_ErrorEvent err;
        MX_ConnEvent conn;
        MX_DiscEvent disc;
    } u;
} MX_Event;

/*
 * An MX component, i.e. a process taking part in a Message Exchange.
 */
typedef struct {
    MListNode _node;                    /* Make it (multi-) listable. */
    char    *name;                      /* Name of this component. */
    char    *host;                      /* Host where it can be reached. */
    uint16_t port;                      /* Its listen port. */
    MX_Id    id;                        /* Its ID number. */
    int      fd;                        /* FD with which we're connected to it. */
    MList    publications;              /* Message types that it publishes. */
    MList    subscriptions;             /* Message types that it subscribes to. */
} MX_Component;

/*
 * An MX message type.
 */
typedef struct {
    MListNode _node;                        /* Make it (multi-) listable. */
    int published_by_me;                    /* Is this message type published by me? */
    char *name;                             /* Name of this message type. */
    MX_Type type;                           /* Numerical type. */
    MList publishers;                       /* The components that publish it. */
    MList subscribers;                      /* The components that subscribe to it. */
    void (*on_message_cb)(MX *mx, int fd, MX_Type type, MX_Version version,
            MX_Size size, char *payload, void *udata);
    const void *on_message_udata;           /* Callback and udata for mxSubscribe. */
    void (*on_new_subscriber_cb)(MX *mx, MX_Type type, int fd, void *udata);
    const void *on_new_subscriber_udata;    /* Callback and udata for mxOnNewSubscriber. */
    void (*on_new_publisher_cb)(MX *mx, MX_Type type, int fd, void *udata);
    const void *on_new_publisher_udata;     /* Callback and udata for mxOnNewPublisher. */
    void (*on_end_subscriber_cb)(MX *mx, MX_Type type, int fd, void *udata);
    const void *on_end_subscriber_udata;    /* Callback and udata for mxOnEndSubscriber. */
    void (*on_end_publisher_cb)(MX *mx, MX_Type type, int fd, void *udata);
    const void *on_end_publisher_udata;     /* Callback and udata for mxOnEndPublisher. */
} MX_Message;

/*
 * A timer, created in an mxOnTime call, or as a timeout in an mxAwait call.
 */
typedef struct {
    ListNode _node;                     /* Make it listable. */

    MX_EventType event_type;            /* Event type we're waiting for. */
    double t;                           /* Time at which to trigger. */

    void (*cb)(MX *mx, double t, void *udata);
    const void *udata;                  /* Callback to call and udata to pass back. */
} MX_Timer;

/*
 * A subscription to non-MX data on a file descriptor, created in an mxOnData call.
 */
typedef struct {
    void (*cb)(MX *mx, int fd, void *udata);
    const void *udata;                  /* Callback and associated udata. */
} MX_Data;

/*
 * A Message Exchange.
 */
struct MX {
    NS ns;                              /* "Base class": a Network Server. */

    int closing_down;                   /* Is this MX closing down? */

    char *mx_name;                      /* Name of this MX. */
    MX_Component *me;                   /* Pointer to the Component that created it. */

    List timers;                        /* List of pending MX_Timers. */
    PointerArray data_by_fd;            /* List of MX_Data structs. */

    List *queue;                        /* Pointer to queue of pending MX_Events. */

    PointerArray component_by_id;       /* Array of components, indexed by id. */
    PointerArray component_by_fd;       /* Array of components, indexed by fd. */

    uint16_t master_port;               /* Port that the master listens on. */
    char *master_host;                  /* Host where the master can be reached. */
    int master_fd;                      /* File descriptor connecting us to the master. */
    int listen_fd;                      /* File descriptor connected to our listen port. */

    HashTable message_by_name;          /* Messages hashed by their name. */
    HashTable message_by_type;          /* Messages hashed by their type. */

    void (*on_new_component_cb)(MX *mx, int fd, const char *name, void *udata);
    const void *on_new_component_udata; /* Callback and udata for mxOnNewSubscriber. */
    void (*on_end_component_cb)(MX *mx, int fd, const char *name, void *udata);
    const void *on_end_component_udata; /* Callback and udata for mxOnEndSubscriber. */
    void (*on_error_cb)(MX *mx, int fd, int error, void *udata);
    const void *on_error_udata;         /* Callback and udata for mxOnError. */
    void (*on_new_message_cb)(MX *mx, const char *msg_name, MX_Type type, void *udata);
    const void *on_new_message_udata;   /* Callback and udata for mxOnNewMessage. */
};

/*
 * Register the fact that component <comp> has a subscription to message <type>.
 */
static void mx_add_subscription(MX *mx, MX_Message *msg, MX_Component *comp)
{
    if (!mlContains(&msg->subscribers, comp))
        mlAppendTail(&msg->subscribers, comp);

    if (!mlContains(&comp->subscriptions, msg))
        mlAppendTail(&comp->subscriptions, msg);

    if (msg->on_new_subscriber_cb && comp != mx->me) {
P       dbgPrint(stderr, "Calling msg->on_new_subscriber_cb\n");

        msg->on_new_subscriber_cb(mx, msg->type, comp->fd, (void *) msg->on_new_subscriber_udata);
    }
}

/*
 * Drop the subscription that component <comp> has to message type <msg>.
 */
static void mx_drop_subscription(MX *mx, MX_Message *msg, MX_Component *comp)
{
    mlRemove(&msg->subscribers, comp);
    mlRemove(&comp->subscriptions, msg);

    if (msg->on_end_subscriber_cb && comp != mx->me) {
        msg->on_end_subscriber_cb(mx, msg->type, comp->fd, (void *) msg->on_end_subscriber_udata);
    }
}

/*
 * Register the fact that component <comp> has a publication to message <type>.
 */
static void mx_add_publication(MX *mx, MX_Message *msg, MX_Component *comp)
{
P   dbgPrint(stderr, "msg = %s, comp = %s\n", msg->name, comp->name);

    if (!mlContains(&msg->publishers, comp))
        mlAppendTail(&msg->publishers, comp);

    if (!mlContains(&comp->publications, msg))
        mlAppendTail(&comp->publications, msg);

    if (msg->on_new_publisher_cb && comp != mx->me) {
P       dbgPrint(stderr, "Calling msg->on_new_publisher_cb\n");

        msg->on_new_publisher_cb(mx, msg->type, comp->fd, (void *) msg->on_new_publisher_udata);
    }
}

/*
 * Drop the publication that component <comp> has of message type <msg>.
 */
static void mx_drop_publication(MX *mx, MX_Message *msg, MX_Component *comp)
{
    mlRemove(&msg->publishers, comp);
    mlRemove(&comp->publications, msg);

    if (msg->on_end_publisher_cb && comp != mx->me) {
        msg->on_end_publisher_cb(mx, msg->type, comp->fd, (void *) msg->on_end_publisher_udata);
    }
}

/*
 * Add a component with id <id> to <mx>.
 */
static MX_Component *mx_add_component(MX *mx, int fd, MX_Id id, const char *name,
        const char *host, int port)
{
    MX_Component *comp = calloc(1, sizeof(MX_Component));

    dbgAssert(stderr, paGet(&mx->component_by_id, id) == NULL, "component %d already exists", id);

    paSet(&mx->component_by_id, id, comp);

    if (fd >= 0) {
        paSet(&mx->component_by_fd, fd, comp);
    }

    comp->name = strdup(name);
    comp->host = strdup(host);
    comp->port = port;
    comp->id   = id;
    comp->fd   = fd;

    if (mx->on_new_component_cb != NULL && fd >= 0) {
        mx->on_new_component_cb(mx, fd, name, (void *) mx->on_new_component_udata);
    }

    return comp;
}

/*
 * Drop component <comp> from <mx>.
 */
static void mx_drop_component(MX *mx, MX_Component *comp)
{
    MX_Message *msg, *next_msg;

    if (mx->on_end_component_cb != NULL && comp->fd >= 0) {
        mx->on_end_component_cb(mx, comp->fd, comp->name, (void *) mx->on_end_component_udata);
    }

    for (msg = mlHead(&comp->publications); msg; msg = next_msg) {
        next_msg = mlNext(&comp->publications, msg);

        mx_drop_publication(mx, msg, comp);
    }

    for (msg = mlHead(&comp->subscriptions); msg; msg = next_msg) {
        next_msg = mlNext(&comp->subscriptions, msg);

        mx_drop_subscription(mx, msg, comp);
    }

    paDrop(&mx->component_by_id, comp->id);

    if (comp->fd >= 0) {
        paDrop(&mx->component_by_fd, comp->fd);
        close(comp->fd);
    }

    free(comp->name);
    free(comp->host);

    free(comp);
}

/*
 * Set the name for message <msg> to <msg_name>.
 */
static void mx_set_message_name(MX *mx, MX_Message *msg, const char *msg_name)
{
    msg->name = strdup(msg_name);
    hashAdd(&mx->message_by_name, msg, HASH_STRING(msg_name));
}

/*
 * Add a message with name <msg_name> and type <msg_type> to <mx>.
 */
static MX_Message *mx_add_message(MX *mx, const char *msg_name, MX_Type msg_type)
{
    MX_Message *msg = calloc(1, sizeof(MX_Message));

    msg->type = msg_type;
    hashAdd(&mx->message_by_type, msg, HASH_VALUE(msg_type));

    if (msg_name != NULL) {
        mx_set_message_name(mx, msg, msg_name);
    }

    return msg;
}

/*
 * Drop the message type <msg>.
 */
static void mx_drop_message(MX *mx, MX_Message *msg)
{
    MX_Component *comp, *next_comp;

    for (comp = mlHead(&msg->subscribers); comp; comp = next_comp) {
        next_comp = mlNext(&msg->subscribers, comp);

        mx_drop_subscription(mx, msg, comp);
    }

    for (comp = mlHead(&msg->publishers); comp; comp = next_comp) {
        next_comp = mlNext(&msg->publishers, comp);

        mx_drop_publication(mx, msg, comp);
    }

    if (msg->name != NULL) {
        hashDel(&mx->message_by_name, HASH_STRING(msg->name));
        free(msg->name);
    }

    hashDel(&mx->message_by_type, HASH_VALUE(msg->type));
    free(msg);
}

/*
 * Create a new, empty message exchange.
 */
static MX *mx_create(void)
{
    MX *mx = calloc(1, sizeof(MX));

    mx->queue = listCreate();

    mx_add_message(mx, "QuitRequest", MX_QUIT_REQUEST);
    mx_add_message(mx, "HelloRequest", MX_HELLO_REQUEST);
    mx_add_message(mx, "HelloReply", MX_HELLO_REPLY);
    mx_add_message(mx, "HelloReport", MX_HELLO_REPORT);
    mx_add_message(mx, "HelloUpdate", MX_HELLO_UPDATE);
    mx_add_message(mx, "RegisterRequest", MX_REGISTER_REQUEST);
    mx_add_message(mx, "RegisterReply", MX_REGISTER_REPLY);
    mx_add_message(mx, "RegisterReport", MX_REGISTER_REPORT);
    mx_add_message(mx, "SubscribeUpdate", MX_SUBSCRIBE_UPDATE);
    mx_add_message(mx, "CancelUpdate", MX_CANCEL_UPDATE);
    mx_add_message(mx, "PublishUpdate", MX_PUBLISH_UPDATE);
    mx_add_message(mx, "WithdrawUpdate", MX_WITHDRAW_UPDATE);

    return mx;
}

/*
 * Create a new event of type <type> and add it to <queue>.
 */
static MX_Event *mx_queue_event(List *queue, MX_EventType type)
{
    MX_Event *evt = calloc(1, sizeof(MX_Event));

    evt->event_type = type;

    listAppendTail(queue, evt);

    return evt;
}

/*
 * Add an error event with the given parameters to <queue>.
 */
static MX_Event *mx_queue_error(List *queue, int fd, int error)
{
    MX_Event *evt = mx_queue_event(queue, MX_ET_ERROR);

    evt->u.err.fd = fd;
    evt->u.err.error = error;

    return evt;
}

/*
 * Add a data event on <fd> to <queue>.
 */
static MX_Event *mx_queue_data(List *queue, int fd)
{
    MX_Event *evt = mx_queue_event(queue, MX_ET_DATA);

    evt->u.data.fd = fd;

    return evt;
}

/*
 * Add a message event with the given parameters to <queue>.
 */
static MX_Event *mx_queue_message(List *queue,
        int fd, MX_Type type, MX_Version version, MX_Size size, const char *payload)
{
    MX_Event *evt = mx_queue_event(queue, MX_ET_MESSAGE);

    evt->u.msg.fd = fd;
    evt->u.msg.type = type;
    evt->u.msg.version = version;
    evt->u.msg.size = size;
    evt->u.msg.payload = memdup(payload, size);

    return evt;
}

/*
 * Tell all connected components that we publish message <msg>.
 */
static void mx_announce_publication(MX *mx, MX_Message *msg)
{
    int fd;

P   dbgPrint(stderr, "Broadcasting publication of %d:", msg->type);

    for (fd = 0; fd < paCount(&mx->component_by_fd); fd++) {
        MX_Component *comp = paGet(&mx->component_by_fd, fd);

        if (comp == NULL) continue;

P       fprintf(stderr, " %s", comp->name);

        mxPack(mx, fd, MX_PUBLISH_UPDATE, 0, PACK_INT32, msg->type, END);
    }

P   fprintf(stderr, "\n");
}

/*
 * Tell all connected components that we publish message <msg>.
 */
static void mx_announce_withdrawal(MX *mx, MX_Message *msg)
{
    int fd;

P   dbgPrint(stderr, "Broadcasting withdrawal of %d:", msg->type);

    for (fd = 0; fd < paCount(&mx->component_by_fd); fd++) {
        MX_Component *comp = paGet(&mx->component_by_fd, fd);

        if (comp == NULL) continue;

P       fprintf(stderr, " %s", comp->name);

        mxPack(mx, fd, MX_WITHDRAW_UPDATE, 0, PACK_INT32, msg->type, END);
    }

P   fprintf(stderr, "\n");
}

/*
 * Tell the component at <fd> about all my subscriptions.
 */
static void mx_send_subscriptions(MX *mx, int fd)
{
    MX_Component *me = mx->me;
    MX_Message *msg;

P   dbgPrint(stderr, "Sending my subscriptions:");

    for (msg = mlHead(&me->subscriptions); msg; msg = mlNext(&me->subscriptions, msg)) {
P       fprintf(stderr, " %s (%d)", msg->name, msg->type);

        mxPack(mx, fd, MX_SUBSCRIBE_UPDATE, 0, PACK_INT32, msg->type, END);
    }

P   fprintf(stderr, "\n");
}

/*
 * Tell the component at <fd> about all my publications.
 */
static void mx_send_publications(MX *mx, int fd)
{
    MX_Component *me = mx->me;
    MX_Message *msg;

P   dbgPrint(stderr, "Sending my publications:");

    for (msg = mlHead(&me->publications); msg; msg = mlNext(&me->publications, msg)) {
P       fprintf(stderr, " %s (%d)", msg->name, msg->type);

        mxPack(mx, fd, MX_PUBLISH_UPDATE, 0, PACK_INT32, msg->type, END);
    }

P   fprintf(stderr, "\n");
}

/*
 * Tell all connected components that we subscribe to message <msg>.
 */
static void mx_announce_subscription(MX *mx, MX_Message *msg)
{
    int fd;

P   dbgPrint(stderr, "Broadcasting subscription of %s (%d):", msg->name, msg->type);

    for (fd = 0; fd < paCount(&mx->component_by_fd); fd++) {
        MX_Component *comp = paGet(&mx->component_by_fd, fd);

        if (comp == NULL) continue;

P       fprintf(stderr, " %s", comp->name);

        mxPack(mx, fd, MX_SUBSCRIBE_UPDATE, 0, PACK_INT32, msg->type, END);
    }

P   fprintf(stderr, "\n");
}

/*
 * Tell all connected components that we subscribe to message <msg>.
 */
static void mx_announce_cancellation(MX *mx, MX_Message *msg)
{
    int fd;

P   dbgPrint(stderr, "Broadcasting cancellation of %s (%d):", msg->name, msg->type);

    for (fd = 0; fd < paCount(&mx->component_by_fd); fd++) {
        MX_Component *comp = paGet(&mx->component_by_fd, fd);

        if (comp == NULL) continue;

P       fprintf(stderr, " %s", comp->name);

        mxPack(mx, fd, MX_CANCEL_UPDATE, 0, PACK_INT32, msg->type, END);
    }

P   fprintf(stderr, "\n");
}

/*
 * The master is telling us about a new component. Handle this.
 */
static void mx_handle_hello_report(MX *mx, int fd,
        MX_Type type, MX_Version version, MX_Size size, char *payload, void *udata)
{
    char *name, *host;
    MX_Id id;
    uint16_t port;

    MX_Component *me = mx->me;

    strunpack(payload, size,
            PACK_INT16,     &id,
            PACK_STRING,    &name,
            PACK_STRING,    &host,
            PACK_INT16,     &port,
            END);

    free(payload);

P   dbgPrint(stderr,
            "Received a HELLO report: id = %d (mine = %d), name = %s, host = %s, port = %d.\n",
            id, me->id, name, host, port);

P   dbgPrint(stderr, "Connecting to component...\n");

    fd = nsConnect(&mx->ns, host, port);

P   dbgPrint(stderr, "Connected on fd %d.\n", fd);

    dbgAssert(stderr, fd >= 0, "nsConnect to component at %s:%d failed.\n", host, port);

P   dbgPrint(stderr, "Adding component...\n");

    mx_add_component(mx, fd, id, name, host, port);

P   dbgPrint(stderr, "Introducing myself...\n");

    mxPack(mx, fd, MX_HELLO_UPDATE, 0,
            PACK_INT16,     me->id,
            PACK_STRING,    me->name,
            PACK_STRING,    me->host,
            PACK_INT16,     me->port,
            END);

    mx_send_subscriptions(mx, fd);
    mx_send_publications(mx, fd);
}

/*
 * The master is telling us about a new message. Handle this.
 */
static void mx_handle_register_report(MX *mx, int fd,
        MX_Type type, MX_Version version, MX_Size size, char *payload, void *udata)
{
    MX_Message *msg;
    MX_Type msg_type;
    char *msg_name;
    int report_it = FALSE;

    strunpack(payload, size,
            PACK_INT32,     &msg_type,
            PACK_STRING,    &msg_name,
            END);

    free(payload);

P   dbgPrint(stderr, "Received a MX_REGISTER_REPORT: type = %d, name = %s.\n", msg_type, msg_name);

    if ((msg = hashGet(&mx->message_by_type, HASH_VALUE(msg_type))) == NULL) {
        mx_add_message(mx, msg_name, msg_type);

P       dbgPrint(stderr, "Didn't know this message. Added.\n");

        report_it = TRUE;
    }
    else if (msg->name == NULL) {
        mx_set_message_name(mx, msg, msg_name);

P       dbgPrint(stderr, "Didn't know this message's name. Set.\n");

        report_it = TRUE;
    }
    else if (strcmp(msg->name, msg_name) != 0) {
        dbgAbort(stderr, "Attempt to change name of message %d from %s to %s\n",
                msg->type, msg->name, msg_name);
    }
    else {
P       dbgPrint(stderr, "Already knew this message. Ignoring.\n");
    }

    if (report_it && mx->on_new_message_cb) {
        mx->on_new_message_cb(mx, msg_name, msg_type, (void *) mx->on_new_message_udata);
    }

    free(msg_name);
}

/*
 * A connected component is introducing themselves to us. Handle this.
 */
static void mx_handle_hello_update(MX *mx, int fd,
        MX_Type type, MX_Version version, MX_Size size, char *payload, void *udata)
{
    MX_Id id;
    char *name, *host;
    uint16_t port;

    strunpack(payload, size,
            PACK_INT16,     &id,
            PACK_STRING,    &name,
            PACK_STRING,    &host,
            PACK_INT16,     &port,
            END);

    free(payload);

P   dbgPrint(stderr, "Received a HELLO update on fd %d: id = %d (mine = %d), name = %s.\n",
            fd, id, mx->me->id, name);

    mx_add_component(mx, fd, id, name, host, port);

    free(name);
    free(host);

    mx_send_publications(mx, fd);
    mx_send_subscriptions(mx, fd);
}

/*
 * A component is telling us about their new subscription. Handle this.
 */
static void mx_handle_subscribe_update(MX *mx, int fd,
        MX_Type type, MX_Version version, MX_Size size, char *payload, void *udata)
{
    MX_Component *comp;
    MX_Message *msg;

P   dbgPrint(stderr, "fd = %d, type = %d\n", fd, type);

    strunpack(payload, size, PACK_INT32, &type, END);

    free(payload);

    comp = paGet(&mx->component_by_fd, fd);

    dbgAssert(stderr, comp != NULL, "Unknown component on fd %d\n", fd);

    if ((msg  = hashGet(&mx->message_by_type, HASH_VALUE(type))) == NULL) {
P       dbgPrint(stderr, "Unknown message type, adding without name.\n");

        msg = mx_add_message(mx, NULL, type);
    }
    else {
P       dbgPrint(stderr, "Known message type (name = %s)\n", msg->name);
    }

P   dbgPrint(stderr, "Adding subscription to %s to %s\n", msg->name, comp->name);

    mx_add_subscription(mx, msg, comp);
}

/*
 * A component is telling us about their cancelled subscription. Handle this.
 */
static void mx_handle_cancel_update(MX *mx, int fd,
        MX_Type type, MX_Version version, MX_Size size, char *payload, void *udata)
{
    MX_Component *comp;
    MX_Message *msg;

    strunpack(payload, size, PACK_INT32, &type, END);

    free(payload);

    comp = paGet(&mx->component_by_fd, fd);
    msg  = hashGet(&mx->message_by_type, HASH_VALUE(type));

    dbgAssert(stderr, comp != NULL, "Unknown component on fd %d\n", fd);
    dbgAssert(stderr, msg != NULL, "Unknown message type %d\n", type);

P   dbgPrint(stderr, "Dropping subscription to %s for %s\n", msg->name, comp->name);

    mx_drop_subscription(mx, msg, comp);
}

/*
 * A component is telling us about their new publication. Handle this.
 */
static void mx_handle_publish_update(MX *mx, int fd,
        MX_Type type, MX_Version version, MX_Size size, char *payload, void *udata)
{
    MX_Component *comp;
    MX_Message *msg;

P   dbgPrint(stderr, "fd = %d, type = %d\n", fd, type);

    strunpack(payload, size, PACK_INT32, &type, END);

    free(payload);

    comp = paGet(&mx->component_by_fd, fd);

    dbgAssert(stderr, comp != NULL, "Unknown component on fd %d\n", fd);

    if ((msg  = hashGet(&mx->message_by_type, HASH_VALUE(type))) == NULL) {
P       dbgPrint(stderr, "Unknown message type, adding without name.\n");

        msg = mx_add_message(mx, NULL, type);
    }
    else {
P       dbgPrint(stderr, "Known message type (name = %s)\n", msg->name);
    }

P   dbgPrint(stderr, "Adding publication of %s for %s\n", msg->name, comp->name);

    mx_add_publication(mx, msg, comp);
}

/*
 * A component is telling us about their withdraw publication. Handle this.
 */
static void mx_handle_withdraw_update(MX *mx, int fd,
        MX_Type type, MX_Version version, MX_Size size, char *payload, void *udata)
{
    MX_Component *comp;
    MX_Message *msg;

    strunpack(payload, size, PACK_INT32, &type, END);

    free(payload);

    comp = paGet(&mx->component_by_fd, fd);
    msg  = hashGet(&mx->message_by_type, HASH_VALUE(type));

    dbgAssert(stderr, comp != NULL, "Unknown component on fd %d\n", fd);
    dbgAssert(stderr, msg != NULL, "Unknown message type %d\n", type);

P   dbgPrint(stderr, "Dropping publication to %s for %s\n", msg->name, comp->name);

    mx_drop_publication(mx, msg, comp);
}

/*
 * Network server <ns> is telling us we have new data on file descriptor <fd>. The current data
 * buffer is at <incoming> and has length <available>. Extract messages and queue events resulting
 * from them.
 */
static void mx_handle_messages(NS *ns, int fd, const char *incoming, int available, void *udata)
{
    MX_Type type;
    MX_Version version;
    MX_Size size;

    MX *mx = (MX *) ns;

P   dbgPrint(stderr, "fd = %d, %d bytes available.\n", fd, available);

    while (1) {
        available = nsAvailable(ns, fd);

P       dbgPrint(stderr, "%d bytes remaining.\n", available);

        if (available < 12) {
P           dbgPrint(stderr, "Too short for a header.\n");
            break;
        }

        incoming  = nsIncoming(ns, fd);

        strunpack(incoming, available,
                PACK_INT32, &type,
                PACK_INT32, &version,
                PACK_INT32, &size,
                END);

P       dbgPrint(stderr, "type = %d, version = %d, size = %d.\n", type, version, size);

        if (available < 12 + size) {
P           dbgPrint(stderr, "Incomplete message.\n");
            break;
        }

P       {
            const char *msg_name = mxMessageName(mx, type);

P           dbgPrint(stderr, "Queueing %s message and discarding %d bytes.\n", msg_name, 12 + size);
        }

        mx_queue_message(mx->queue, fd, type, version, size, incoming + 12);

        nsDiscard(ns, fd, 12 + size);
    }
}

/*
 * Handle a disconnect on fd <fd>. If it turns out to be the file descriptor for the connection to
 * the master component, panic and exit. Otherwise clean up whatever component was connected on this
 * file descriptor.
 */
static void mx_handle_disconnect(NS *ns, int fd, void *udata)
{

    MX_Component *comp;
    MX *mx = (MX *) ns;

    if (fd == mx->master_fd) {
P       dbgPrint(stderr, "Lost connection with the master, exiting.\n");
        mxClose(mx);
    }
    else if ((comp = paGet(&mx->component_by_fd, fd)) != NULL) {
        mx_drop_component(mx, comp);
    }
}

/*
 * Handle an error on file descriptor <fd> reported by <ns>. The errno code is contained in <error>.
 */
static void mx_handle_error(NS *ns, int fd, int error, void *udata)
{
    MX *mx = (MX *) ns;

    mx_queue_error(mx->queue, fd, error);
}

/*
 * Handle non-MX data on file descriptor <fd> in <ns>.
 */
static void mx_handle_data(NS *ns, int fd, void *udata)
{
    MX *mx = (MX *) ns;

    mx_queue_data(mx->queue, fd);
}

/*
 * Handle a timer at time <t> for <ns>.
 */
static void mx_handle_timer(NS *ns, double t, void *udata)
{
    MX *mx = (MX *) ns;
    MX_Timer *timer = listHead(&mx->timers);

    mx_queue_event(mx->queue, timer->event_type);
}

/*
 * Add a timer at time <t> to <mx>. The event type this timer waits for is in <type> and <cb> is the
 * callback to call when this timer goes off.
 */
static MX_Timer *mx_add_timer(MX *mx, double t, MX_EventType event_type,
        void (*cb)(MX *mx, double t, void *udata), const void *udata)
{
    MX_Timer *timer, *new_timer = calloc(1, sizeof(MX_Timer));

    new_timer->event_type = event_type;
    new_timer->t = t;
    new_timer->cb = cb;
    new_timer->udata = udata;

    for (timer = listHead(&mx->timers); timer; timer = listNext(timer)) {
        if (timer->t > new_timer->t) break;
    }

    listInsert(&mx->timers, new_timer, timer);

    nsOnTime(&mx->ns, t, mx_handle_timer, NULL);

    return timer;
}

/*
 * Connect to the master component for message exchange <mx_name> on host <mx_host>. Introduce
 * myself as <my_name>. Returns a pointer to an MX struct. If <mx_name> is NULL, the environment
 * variable MX_NAME is used, or, if it doesn't exist, the current user name. If <mx_host> is NULL
 * it is set to the environment variable MX_HOST, or, if it doesn't exist, to "localhost". <my_name>
 * can not be NULL.
 */
MX *mxConnect(const char *mx_name, const char *mx_host, const char *my_name)
{
    int r;

    MX_Version version;
    MX_Size size;
    char *payload;
    MX_Id id;

    uint16_t listen_port;
    const char *listen_host;

    MX *mx = mx_create();

    nsOnSocket(&mx->ns, mx_handle_messages, NULL);
    nsOnDisconnect(&mx->ns, mx_handle_disconnect, NULL);
    nsOnError(&mx->ns, mx_handle_error, NULL);

    mx->mx_name = strdup(mx_get_name(mx_name));

    mx->master_host = strdup(mx_get_host(mx_host));
    mx->master_port = mx_get_port(mx->mx_name);
    mx->master_fd   = nsConnect(&mx->ns, mx->master_host, mx->master_port);

    if (mx->master_fd < 0) {
        fprintf(stderr, "Could not connect to master at %s:%d\n",
                mx->master_host, mx->master_port);
        exit(1);
    }

    mx->listen_fd = nsListen(&mx->ns, NULL, 0);

    if (mx->listen_fd < 0) {
        fprintf(stderr, "Could not open a listen port\n");
        exit(1);
    }

    listen_port = netLocalPort(mx->listen_fd);
    listen_host = netLocalHost(mx->listen_fd);

P   dbgPrint(stderr, "listen_port = %d\n", listen_port);
P   dbgPrint(stderr, "listen_host = %s\n", listen_host);

    dbgAssert(stderr, my_name != NULL, "my_name can not be NULL in mxConnect.\n");

    mxPack(mx, mx->master_fd, MX_HELLO_REQUEST, 0,
            PACK_STRING,    my_name,
            PACK_INT16,     listen_port,
            END);

    r = mxAwait(mx, mx->master_fd, MX_HELLO_REPLY, &version, &size, &payload, 3600);

    if (r != 0) {
        fprintf(stderr, "%s waiting for reply to hello message\n", r == -1 ? "Error" : "Timeout");
        exit(1);
    }

    strunpack(payload, size, PACK_INT16, &id, END);

    free(payload);

    mx->me = mx_add_component(mx, -1, id, my_name, listen_host, listen_port);

    mx->me->name = strdup(my_name);
    mx->me->host = strdup(listen_host);
    mx->me->port = listen_port;

    mxSubscribe(mx, MX_HELLO_REPORT, mx_handle_hello_report, NULL);
    mxSubscribe(mx, MX_HELLO_UPDATE, mx_handle_hello_update, NULL);
    mxSubscribe(mx, MX_REGISTER_REPORT, mx_handle_register_report, NULL);
    mxSubscribe(mx, MX_SUBSCRIBE_UPDATE, mx_handle_subscribe_update, NULL);
    mxSubscribe(mx, MX_CANCEL_UPDATE, mx_handle_cancel_update, NULL);
    mxSubscribe(mx, MX_PUBLISH_UPDATE, mx_handle_publish_update, NULL);
    mxSubscribe(mx, MX_WITHDRAW_UPDATE, mx_handle_withdraw_update, NULL);

    return mx;
}

/*
 * Register a message type with name <msg_name> using <mx>, and return the associated message type
 * id.
 */
MX_Type mxRegister(MX *mx, const char *msg_name)
{
    int r;
    MX_Version version;
    MX_Size size;
    MX_Message *msg;
    MX_Type msg_type;
    char *payload;

P   dbgPrint(stderr, "msg_name = %s\n", msg_name);

    if ((msg = hashGet(&mx->message_by_name, HASH_STRING(msg_name))) != NULL) {
P       dbgPrint(stderr, "Already know this message. Returning existing type.\n");

        return msg->type;
    }

    mxPack(mx, mx->master_fd, MX_REGISTER_REQUEST, 0,
            PACK_STRING,    msg_name,
            END);

    r = mxAwait(mx, mx->master_fd, MX_REGISTER_REPLY, &version, &size, &payload, 5);

    if (r != 0) {
        fprintf(stderr, "%s waiting for reply to register message\n",
                r == -1 ? "Error" : "Timeout");
        exit(1);
    }

    strunpack(payload, size, PACK_INT32, &msg_type, END);

    free(payload);

    if ((msg = hashGet(&mx->message_by_type, HASH_VALUE(msg_type))) != NULL) {
        mx_set_message_name(mx, msg, msg_name);

        return msg->type;
    }
    else {
        mx_add_message(mx, msg_name, msg_type);

        return msg_type;
    }
}

/*
 * Subscribe to messages of type <type> through <mx>. When a message of this type comes in call <cb>
 * with the properties of this message and the <udata> that was passed in here. The payload that is
 * passed to <cb> is the callee's responsibility, and they should free it when it is no longer
 * needed.
 */
void mxSubscribe(MX *mx, MX_Type type,
        void (*cb)(MX *mx, int fd, MX_Type type, MX_Version version, MX_Size size,
            char *payload, void *udata),
        void *udata)
{
    MX_Message *msg = hashGet(&mx->message_by_type, HASH_VALUE(type));

    msg->on_message_cb = cb;
    msg->on_message_udata = udata;

    mx_add_subscription(mx, msg, mx->me);
    mx_announce_subscription(mx, msg);
}

/*
 * Cancel your subscription to messages of type <type>.
 */
void mxCancel(MX *mx, MX_Type type)
{
    MX_Message *msg = hashGet(&mx->message_by_type, HASH_VALUE(type));

    msg->on_message_cb = NULL;
    msg->on_message_udata = NULL;

    mx_drop_subscription(mx, msg, mx->me);
    mx_announce_cancellation(mx, msg);
}

/*
 * Announce that you'll be broadcasting messages of type <type>. This function must be called before
 * using mxBroadcast (or mxVaBroadcast). It is not necessary (but allowed) if you only use mxWrite,
 * mxPack or mxVaPack for this message type.
 */
void mxPublish(MX *mx, MX_Type type)
{
    MX_Message *msg = hashGet(&mx->message_by_type, HASH_VALUE(type));

    msg->published_by_me = TRUE;

    mx_add_publication(mx, msg, mx->me);
    mx_announce_publication(mx, msg);
}

/*
 * Withdraw the publication of messages of type <type>. No more messages of this type may be
 * broadcast after a call to this function.
 */
void mxWithdraw(MX *mx, MX_Type type)
{
    MX_Message *msg = hashGet(&mx->message_by_type, HASH_VALUE(type));

    msg->published_by_me = FALSE;

    mx_drop_publication(mx, msg, mx->me);
    mx_announce_withdrawal(mx, msg);
}

/*
 * Add a timer at time <t> to <mx>, and call <cb> with <udata> when it goes off.
 */
void mxOnTime(MX *mx, double t, void (*cb)(MX *mx, double t, void *udata), const void *udata)
{
    if (mx->closing_down) return;

    dbgAssert(stderr, cb != NULL, "<cb> can not be NULL in mxOnTime()");

    mx_add_timer(mx, t, MX_ET_TIMER, cb, udata);
}

/*
 * Drop the previously added timer at time <t> with callback <cb> in <mx>. Both <t> and <cb> must
 * match the previously set timer.
 */
void mxDropTime(MX *mx, double t, void (*cb)(MX *mx, double t, void *udata))
{
    MX_Timer *timer;

    nsDropTime(&mx->ns, t, mx_handle_timer);

    for (timer = listHead(&mx->timers); timer; timer = listNext(timer)) {
        if (timer->t == t && timer->cb == cb) {
            listRemove(&mx->timers, timer);
            free(timer);

            break;
        }
    }
}

/*
 * Tell <mx> to call <cb> with <udata> when file descriptor <fd> has data avialable. <fd> in this
 * case represents a file descriptor opened by the user themselves, not one managed by <mx>.
 */
void mxOnData(MX *mx, int fd, void (*cb)(MX *mx, int fd, void *udata), const void *udata)
{
    MX_Data *data;

    if (mx->closing_down) return;

    data = calloc(1, sizeof(MX_Data));

    data->cb = cb;
    data->udata = udata;

    paSet(&mx->data_by_fd, fd, data);

    nsOnData(&mx->ns, fd, mx_handle_data, udata);
}

/*
 * Tell <mx> to stop listening for data on <fd>. Again, <fd> is a file descriptor opened by the
 * user, not one managed by <mx>.
 */
void mxDropData(MX *mx, int fd)
{
    MX_Data *data = paGet(&mx->data_by_fd, fd);

    if (data == NULL) return;

    paDrop(&mx->data_by_fd, fd);

    nsDropData(&mx->ns, fd);

    free(data);
}

/*
 * Call <cb> when a new component reports in. The file descriptor that the component is connected to
 * and its name are passed to <cb>, along with <udata>.
 */
void mxOnNewComponent(MX *mx,
        void (*cb)(MX *mx, int fd, const char *name, void *udata), const void *udata)
{
P   dbgPrint(stderr, "cb = %p, udata = %p\n", cb, udata);

    mx->on_new_component_cb = cb;
    mx->on_new_component_udata = udata;
}

/*
 * Call <cb> when a component disconnects. The file descriptor where it was connected and its name
 * are passed to <cb> along with <udata>.
 */
void mxOnEndComponent(MX *mx,
        void (*cb)(MX *mx, int fd, const char *name, void *udata), const void *udata)
{
P   dbgPrint(stderr, "cb = %p\n", cb);

    mx->on_end_component_cb = cb;
    mx->on_end_component_udata = udata;
}

/*
 * Arrange for <cb> to be called if a new subscriber on message type <type> announces itself. Not
 * called for own subscriptions created with mxSubscribe.
 */
void mxOnNewSubscriber(MX *mx, MX_Type type,
        void (*cb)(MX *mx, MX_Type type, int fd, void *udata), const void *udata)
{
    MX_Message *msg = hashGet(&mx->message_by_type, HASH_VALUE(type));
    MX_Component *comp;

P   dbgPrint(stderr, "type = %d, msg = %s\n", type, msg->name);

    msg->on_new_subscriber_cb = cb;
    msg->on_new_subscriber_udata = udata;

    for (comp = mlHead(&msg->subscribers); comp; comp = mlNext(&msg->subscribers, comp)) {
P       dbgPrint(stderr, "Calling msg->on_new_subscriber_cb\n");

        msg->on_new_subscriber_cb(mx, type, comp->fd, (void *) msg->on_new_subscriber_udata);
    }
}

/*
 * Arrange for <cb> to be called if a new publisher of message type <type> announces itself. Not
 * called for own subscriptions created with mxPublish.
 */
void mxOnNewPublisher(MX *mx, MX_Type type,
        void (*cb)(MX *mx, MX_Type type, int fd, void *udata), const void *udata)
{
    MX_Message *msg = hashGet(&mx->message_by_type, HASH_VALUE(type));
    MX_Component *comp;

P   dbgPrint(stderr, "type = %d, msg = %s\n", type, msg->name);

    msg->on_new_publisher_cb = cb;
    msg->on_new_publisher_udata = udata;

    for (comp = mlHead(&msg->publishers); comp; comp = mlNext(&msg->publishers, comp)) {
P       dbgPrint(stderr, "Calling msg->on_new_publisher_cb\n");

        msg->on_new_publisher_cb(mx, type, comp->fd, (void *) msg->on_new_publisher_udata);
    }
}

/*
 * Arrange for <cb> to be called if a subscriber to messages of type <type> exits or cancels its
 * subscription. Not called on mxCancel.
 */
void mxOnEndSubscriber(MX *mx, MX_Type type,
        void (*cb)(MX *mx, MX_Type type, int fd, void *udata), const void *udata)
{
    MX_Message *msg = hashGet(&mx->message_by_type, HASH_VALUE(type));

P   dbgPrint(stderr, "type = %d, msg = %s\n", type, msg->name);

    msg->on_end_subscriber_cb = cb;
    msg->on_end_subscriber_udata = udata;
}

/*
 * Arrange for <cb> to be called if a publisher of messages of type <type> exits or withdraws its
 * publication. Not called on mxWithdraw.
 */
void mxOnEndPublisher(MX *mx, MX_Type type,
        void (*cb)(MX *mx, MX_Type type, int fd, void *udata), const void *udata)
{
    MX_Message *msg = hashGet(&mx->message_by_type, HASH_VALUE(type));

P   dbgPrint(stderr, "type = %d, msg = %s\n", type, msg->name);

    msg->on_end_publisher_cb = cb;
    msg->on_end_publisher_udata = udata;
}

/*
 * Call <cb> when a new message is registered.
 */
void mxOnNewMessage(MX *mx,
        void (*cb)(MX *mx, const char *name, MX_Type type, void *udata), const void *udata)
{
P   dbgPrint(stderr, "cb = %p, udata = %p\n", cb, udata);

    mx->on_new_message_cb = cb;
    mx->on_new_message_udata = udata;
}

/*
 * Return the number of publishers of messages of type <type>.
 */
int mxPublisherCount(MX *mx, MX_Type type)
{
    MX_Message *msg = hashGet(&mx->message_by_type, HASH_VALUE(type));

    return mlLength(&msg->publishers);
}

/*
 * Return the number of subscribers to messages of type <type>.
 */
int mxSubscriberCount(MX *mx, MX_Type type)
{
    MX_Message *msg = hashGet(&mx->message_by_type, HASH_VALUE(type));

    return mlLength(&msg->subscribers);
}

/*
 * Return the name of the component at file descriptor <fd> on <mx>.
 */
const char *mxComponentName(MX *mx, int fd)
{
    MX_Component *comp = paGet(&mx->component_by_fd, fd);

    dbgAssert(stderr, comp != NULL, "Component on fd %d unknown.\n", fd);

    return comp->name;
}

/*
 * Return the name of message type <type>.
 */
const char *mxMessageName(MX *mx, MX_Type type)
{
    MX_Message *msg = hashGet(&mx->message_by_type, HASH_VALUE(type));

    dbgAssert(stderr, msg != NULL, "Message type %d unknown.\n", type);

    return msg->name;
}

/*
 * Write <payload>, which has size <size>, in a message of type <type> with version <version to file
 * descriptor <fd> over message exchange <mx>.
 */
void mxWrite(MX *mx, int fd, MX_Type type, MX_Version version, const char *payload, size_t size)
{
    nsPack(&mx->ns, fd,
            PACK_INT32, type,
            PACK_INT32, version,
            PACK_DATA,  payload, size,
            END);
}

/*
 * Write a message of type <type> with version <version> to file descriptor <fd> over message
 * exchange <mx>. The payload of the message is constructed using the PACK_* method as described in
 * libjvs/utils.h.
 */
void mxPack(MX *mx, int fd, MX_Type type, MX_Version version, ...)
{
    va_list ap;

    va_start(ap, version);
    mxVaPack(mx, fd, type, version, ap);
    va_end(ap);
}

/*
 * Write a message of type <type> with version <version> to file descriptor <fd> over message
 * exchange <mx>. The payload of the message is constructed using the PACK_* arguments contained in
 * <ap>, as described in libjvs/utils.h.
 */
void mxVaPack(MX *mx, int fd, MX_Type type, MX_Version version, va_list ap)
{
    char *payload;

    int size = vastrpack(&payload, ap);

    mxWrite(mx, fd, type, version, payload, size);

    free(payload);
}

/*
 * Broadcast a message with type <type> and version <version> to all subscribers of this message
 * type. The contents of the message are set using the "astrpack" interface from libjvs/utils.h.
 * Publication of this message type must have been announced with the mxPublish function.
 */
void mxBroadcast(MX *mx, MX_Type type, MX_Version version, ...)
{
    va_list ap;

    va_start(ap, version);
    mxVaBroadcast(mx, type, version, ap);
    va_end(ap);
}

/*
 * Broadcast a message with type <type> and version <version> to all subscribers of this message
 * type. The contents of the message are set using the "vastrpack" interface from libjvs/utils.h.
 * Publication of this message type must have been announced with the mxPublish function.
 */
void mxVaBroadcast(MX *mx, MX_Type type, MX_Version version, va_list ap)
{
    char *payload;
    MX_Component *comp;

    MX_Message *msg = hashGet(&mx->message_by_type, HASH_VALUE(type));

    int size = vastrpack(&payload, ap);

    dbgAssert(stderr, msg->published_by_me,
            "Broadcast of message type %d (%s) but mxPublish never called", type, msg->name);

    for (comp = mlHead(&msg->subscribers); comp; comp = mlNext(&msg->subscribers, comp)) {
        mxWrite(mx, comp->fd, type, version, payload, size);
    }

    free(payload);
}

/*
 * Wait for a message of type <type> to arrive over file descriptor <fd> on message exchange <mx>.
 * The version of the received message is returned through <version> and its payload through
 * <payload>. If the message arrives within <timeout> seconds, the function returns 0. If it doesn't
 * it returns 1. If some other (network) error occurs, it returns -1.
 */
int mxAwait(MX *mx, int fd, MX_Type type, MX_Version *version, MX_Size *size, char **payload,
        double timeout)
{
    MX_Event *evt, *last_evt = NULL;
    List *saved_queue, local_queue = { 0 };
    double t = nowd() + timeout;
    int r;

P   dbgPrint(stderr, "Looking for %s message on fd %d, timeout in %f seconds.\n",
            mxMessageName(mx, type), fd, timeout);

    mx_add_timer(mx, t, MX_ET_AWAIT, NULL, NULL);

    saved_queue = mx->queue;
    mx->queue = &local_queue;

    for ever {
P       dbgPrint(stderr, "Calling nsHandleEvents\n");

        r = nsHandleEvents(&mx->ns);

P       dbgPrint(stderr, "nsHandleEvents returned %d\n", r);

        if (r != 0) break;

        if (listLength(mx->queue) == 0) {
P           dbgPrint(stderr, "No events in queue, retrying.\n");
            continue;
        }

P       dbgPrint(stderr, "%d events in queue.\n", listLength(mx->queue));

        if (last_evt == NULL) {
P           dbgPrint(stderr, "Starting from head of the queue.\n");
            evt = listHead(&local_queue);
        }
        else {
P           dbgPrint(stderr, "Continuing from last_evt.\n");
            evt = listNext(last_evt);
        }

        if (evt == NULL) {
P           dbgPrint(stderr, "No new events added to queue, retrying.\n");
            continue;
        }

        for ( ; evt; evt = listNext(evt)) {
P           dbgPrint(stderr, "Event of type %d.\n", evt->event_type);

            if (evt->event_type == MX_ET_AWAIT) {
P               dbgPrint(stderr, "AWAIT timer, ending the loop.\n");
                break;
            }
            else if (evt->event_type != MX_ET_MESSAGE) {
P               dbgPrint(stderr, "Not an incoming message, continuing\n");
            }
            else if (evt->u.msg.fd != fd) {
P               dbgPrint(stderr, "Message on fd %d.\n", evt->u.msg.fd);
            }
            else if (evt->u.msg.type != type) {
P               dbgPrint(stderr, "Message on fd %d, but type %d.\n", fd, evt->u.msg.type);
            }
            else {
P               dbgPrint(stderr, "Found what I was looking for, ending the loop\n");
                break;
            }

            last_evt = evt;
        }

        if (evt == NULL) {
P           dbgPrint(stderr, "Requested message not found, retrying.\n");
            continue;
        }

        listRemove(&local_queue, evt);

        if (evt->event_type == MX_ET_AWAIT) {
            r = 1;

            free(evt);

            break;
        }
        else {
            *version = evt->u.msg.version;
            *size    = evt->u.msg.size;
            *payload = evt->u.msg.payload;

            free(evt);

            break;
        }
    }

    mx->queue = saved_queue;

    mxDropTime(mx, t, NULL);

P   dbgPrint(stderr, "Copying %d events from local queue to MX queue.\n", listLength(&local_queue));

    while ((evt = listRemoveHead(&local_queue)) != NULL) {
        listAppendTail(mx->queue, evt);
    }

    return r;
}

/*
 * Run the message exchange <mx>. This function will return 0 if there are no more timers or
 * connections to wait for, or -1 if an error occurred.
 */
int mxRun(MX *mx)
{
    int r;

    MX_Data *data;
    MX_Message *msg;
    MX_Timer *timer;

    for ever {
        MX_Event *evt;

P       dbgPrint(stderr, "%d events in the queue.\n", listLength(mx->queue));

        if (listLength(mx->queue) == 0) {
P           dbgPrint(stderr, "Calling nsHandleEvents.\n");

            if ((r = nsHandleEvents(&mx->ns)) != 0) {
P               dbgPrint(stderr, "nsHandleEvents returned %d, ending the loop.\n", r);
                break;
            }

P           dbgPrint(stderr, "nsHandleEvents collected %d events.\n", listLength(mx->queue));
        }

        while ((evt = listRemoveHead(mx->queue)) != NULL) {
P           dbgPrint(stderr, "Event type %d\n", evt->event_type);

            switch(evt->event_type) {
            case MX_ET_DATA:
                data = paGet(&mx->data_by_fd, evt->u.data.fd);
                data->cb(mx, evt->u.data.fd, (void *) data->udata);
                break;
            case MX_ET_MESSAGE:
                msg = hashGet(&mx->message_by_type, HASH_VALUE(evt->u.msg.type));
                msg->on_message_cb(mx, evt->u.msg.fd, evt->u.msg.type, evt->u.msg.version,
                        evt->u.msg.size, evt->u.msg.payload, (void *) msg->on_message_udata);
                break;
            case MX_ET_TIMER:
                timer = listRemoveHead(&mx->timers);
                timer->cb(mx, timer->t, (void *) timer->udata);
                free(timer);
                break;
            case MX_ET_ERROR:
                if (mx->on_error_cb != NULL) {
                    mx->on_error_cb(mx, evt->u.err.fd, evt->u.err.error,
                            (void *) mx->on_error_udata);
                }
                break;
            default:
                fprintf(stderr, "Unexpected event (type %d)\n", evt->event_type);
                break;
            }

            free(evt);
        }
    }

    return r > 0 ? 0 : r;
}

/*
 * Close down message exchange <mx>. This will cause a call to mxRun or mxAwait to terminate.
 */
void mxClose(MX *mx)
{
    nsClose(&mx->ns);

    mx->closing_down = TRUE;
}

/*
 * Destroy message exchange <mx>. Do not call this inside mxRun. Instead, call mxClose and wait for
 * mxRun to terminate, then call mxDestroy.
 */
void mxDestroy(MX *mx)
{
    int id, i;

    void *ptr;

    free(mx->mx_name);
    free(mx->master_host);

    for (id = 0; id < paCount(&mx->component_by_id); id++) {
        MX_Component *comp = paGet(&mx->component_by_id, id);

        if (comp != NULL) mx_drop_component(mx, comp);
    }

    for (i = hashFirst(&mx->message_by_type, &ptr); i; i = hashNext(&mx->message_by_type, &ptr)) {
        MX_Message *msg = ptr;

        mx_drop_message(mx, msg);
    }

    paClear(&mx->component_by_id);
    paClear(&mx->component_by_fd);

    hashClearTable(&mx->message_by_name);
    hashClearTable(&mx->message_by_type);

    nsClear(&mx->ns);

    free(mx);
}
