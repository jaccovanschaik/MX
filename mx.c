/*
 * Message Exchange command line tool.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: mx.c 123 2013-10-17 18:41:53Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include <libjvs/ns.h>
#include <libjvs/net.h>
#include <libjvs/tcp.h>
#include <libjvs/pa.h>
#include <libjvs/hash.h>
#include <libjvs/debug.h>
#include <libjvs/utils.h>
#include <libjvs/buffer.h>
#include <libjvs/options.h>

#include "private.h"

static MX_Id next_component_id = 0;
static MX_Type next_message_type = NUM_MX_MESSAGES;

static PointerArray component_by_fd = { };
static HashTable messages = { };

/*
 * A message type.
 */
typedef struct {
    MX_Type type;                       /* Numerical type. */
    char *name;                         /* Name of this message type. */
} Message;

/*
 * A connected component.
 */
typedef struct {
    char     *name;                     /* Name of this component. */
    char     *host;                     /* Host name where it can be reached. */
    uint16_t  port;                     /* TCP port that it is listening on. */
    MX_Id     id;                       /* ID number. */
    int       fd;                       /* File descriptor on which it is connected. */
} Component;

/* === Usage === */

static void usage(FILE *fp, const char *argv0, int exit_code)
{
    fprintf(fp, "This is the Message Exchange command line tool.\n\n");

    fprintf(fp, "Usage: %s <subcommand> [options]\n\n", argv0);

    fprintf(fp, "Subcommands:\n");

    fprintf(fp, "\thelp\tshow this help.\n");
    fprintf(fp, "\tmaster\trun the master component.\n");
    fprintf(fp, "\tquit\task the master component to quit.\n");
    fprintf(fp, "\tname\tshow the ns name being used.\n");
    fprintf(fp, "\tport\tshow the master component's listen port.\n");

    exit(exit_code);
}

/*
 * Send a message over file descriptor <fd> via network server <ns>. The message will have type
 * <type>, version <version>. Its payload is created using the PACK_* arguments from libjvs/utils.h
 * contained in <ap>.
 */
static void va_send_message(NS *ns, int fd, MX_Type type, MX_Version version, va_list ap)
{
    char *payload;

    int n = vastrpack(&payload, ap);

    nsPack(ns, fd,
            PACK_INT32, type,
            PACK_INT32, version,
            PACK_INT32, n,
            PACK_RAW,   payload, n,
            END);

    free(payload);
}

/*
 * Send a message over file descriptor <fd> via network server <ns>. The message will have type
 * <type>, version <version>. Its payload is created using the PACK_* arguments from libjvs/utils.h
 * that follow <version>.
 */
static void send_message(NS *ns, int fd, MX_Type type, MX_Version version, ...)
{
    va_list ap;

    va_start(ap, version);
    va_send_message(ns, fd, type, version, ap);
    va_end(ap);
}

/* === Functions for the master subcommand === */

/*
 * Handle a QUIT request: terminate the master.
 */
static void master_handle_quit_request(NS *ns)
{
    fprintf(stderr, "Received QUIT request. Closing down.\n");

    nsClose(ns);
}

/*
 * Create a new component with name <name> and id <id>. It is connected to us via file descriptor
 * <fd> and it accepts new connections on port <port> at host <host>.
 */
static Component *master_new_component(int id, int fd, const char *name, const char *host, int port)
{
    Component *comp = calloc(1, sizeof(Component));

    comp->fd = fd;
    comp->id = id;

    comp->name = strdup(name);
    comp->host = strdup(host);
    comp->port = port;

    paSet(&component_by_fd, fd, comp);

    return comp;
}

/*
 * Drop component <comp>.
 */
static void master_drop_component(NS *ns, Component *comp)
{
    paDrop(&component_by_fd, comp->fd);

    free(comp->name);
    free(comp->host);

    free(comp);
}

/*
 * Handle a HELLO request coming in on file descriptor <fd>.
 */
static void master_handle_hello_request(NS *ns, int fd, MX_Version version,
        MX_Size size, const char *payload)
{
    char *name;
    int i;
    uint16_t port;

    const char *host = netPeerHost(fd);

    Component *new_comp;

    strunpack(payload, size,
            PACK_STRING,    &name,
            PACK_INT16,     &port,
            END);

P   dbgPrint(stderr, "Received HELLO: name = %s\n", name);
P   dbgPrint(stderr, "Received HELLO: port = %d\n", port);

    new_comp = master_new_component(++next_component_id, fd, name, host, port);

    send_message(ns, fd, MX_HELLO_REPLY, 0,
            PACK_INT16, next_component_id,
            END);

    for (i = 0; i < paCount(&component_by_fd); i++) {
        Component *comp;

        if ((comp = paGet(&component_by_fd, i)) == NULL || comp == new_comp) continue;

P       dbgPrint(stderr,
                "Sending MX_HELLO_REPORT to fd %d: id = %d, name = %s, host = %s, port = %d\n",
                fd, comp->id, comp->name, comp->host, comp->port);

        send_message(ns, fd, MX_HELLO_REPORT, 0,
                PACK_INT16,     comp->id,
                PACK_STRING,    comp->name,
                PACK_STRING,    comp->host,
                PACK_INT16,     comp->port,
                END);
    }

    for (i = 0; i < next_message_type; i++) {
        Message *msg;

        if ((msg = hashGet(&messages, HASH_VALUE(i))) != NULL) {
P           dbgPrint(stderr, "Sending MX_REGISTER_REPORT to fd %d: type = %d, name = %s\n",
                    fd, msg->type, msg->name);

            send_message(ns, fd, MX_REGISTER_REPORT, 0,
                    PACK_INT32,     msg->type,
                    PACK_STRING,    msg->name,
                    END);
        }
    }

    free(name);
}

/*
 * Handle a REGISTER request coming in on file descriptor <fd>.
 */
static void master_handle_register_request(NS *ns, int fd, MX_Version version,
        MX_Size size, const char *payload)
{
    char *name;
    int i;
    Message *msg;

    strunpack(payload, size,
            PACK_STRING, &name,
            END);

P   dbgPrint(stderr, "name = %s\n", name);

    if ((msg = hashGet(&messages, HASH_STRING(name))) == NULL) {
P       dbgPrint(stderr, "No such message, creating.\n");

        msg = calloc(1, sizeof(Message));

        msg->type = next_message_type++;
        msg->name = strdup(name);

        hashAdd(&messages, msg, HASH_STRING(name));

        for (i = 0; i < paCount(&component_by_fd); i++) {
            Component *comp;

            if (i == fd || (comp = paGet(&component_by_fd, i)) == NULL) continue;

P           dbgPrint(stderr, "Sending MX_REGISTER_REPORT to fd %d: type = %d, name = %s\n",
                    i, msg->type, msg->name);

            send_message(ns, i, MX_REGISTER_REPORT, 0,
                    PACK_INT32,     msg->type,
                    PACK_STRING,    msg->name,
                    END);
        }
    }
    else {
P       dbgPrint(stderr, "Message exists.\n");
    }

P   dbgPrint(stderr, "Returning message type = %d\n", msg->type);

    send_message(ns, fd, MX_REGISTER_REPLY, 0, PACK_INT32, msg->type, END);

    free(name);
}

/*
 * Handle a lost connection on file descriptor <fd>.
 */
static void master_on_disconnect(NS *ns, int fd, void *udata)
{
    Component *comp = paGet(&component_by_fd, fd);

P   dbgPrint(stderr, "Lost connection on fd %d\n", fd);

    if (comp) master_drop_component(ns, comp);
}

/*
 * Handle incoming socket data on <fd>.
 */
static void master_handle_socket_data(NS *ns, int fd, const char *incoming, int available,
        void *udata)
{
    MX_Type type;
    MX_Version version;
    MX_Size size;
    const char *payload;

    while (1) {
        available = nsAvailable(ns, fd);
        incoming  = nsIncoming(ns, fd);

        if (available < 12) break;

        strunpack(incoming, available,
                PACK_INT32, &type,
                PACK_INT32, &version,
                PACK_INT32, &size,
                END);

        if (available < 12 + size) break;

        payload = incoming + 12;

        switch(type) {
        case MX_QUIT_REQUEST:
            master_handle_quit_request(ns);
            break;
        case MX_HELLO_REQUEST:
            master_handle_hello_request(ns, fd, version, size, payload);
            break;
        case MX_REGISTER_REQUEST:
            master_handle_register_request(ns, fd, version, size, payload);
            break;
        default:
            break;
        }

        nsDiscard(ns, fd, 12 + size);
    }
}

/*
 * Run the master subcommand.
 */
static int cmd_master(int argc, char *argv[])
{
    int r;
    NS *ns;
    const char *name;
    uint16_t port;

    Options *options = optCreate();

    optAdd(options, "name", 'n', ARG_REQUIRED);
    optAdd(options, "foreground", 'f', ARG_NONE);

    if (optParse(options, argc, argv) == -1) return -1;

    ns = nsCreate();

    name = mx_get_name(optArg(options, "name"));
    port = mx_get_port(name);

    if (nsListen(ns, NULL, port) < 0) {
        dbgError(stderr, "Could not open listen socket on port %d", port);
        return 1;
    }

    nsOnDisconnect(ns, master_on_disconnect, NULL);
    nsOnSocket(ns, master_handle_socket_data, NULL);

    if (!optIsSet(options, "foreground") && daemon(0, 0) != 0) {
        dbgError(stderr, "daemon() failed");
        return -1;
    }
    else {
        fprintf(stderr, "Master listening on port %d for mx \"%s\"\n", port, name);
    }

    r = nsRun(ns);

    nsDestroy(ns);

    return r;
}

/* === The quit subcommand === */

static int cmd_quit(int argc, char *argv[])
{
    const char *name, *host;
    char *str;
    uint16_t port;
    int fd, size, r;
    fd_set rfds;
    struct timeval tv;
    int verbose;

    Options *options = optCreate();

    optAdd(options, "name", 'n', ARG_REQUIRED);
    optAdd(options, "host", 'h', ARG_REQUIRED);
    optAdd(options, "verbose", 'v', ARG_NONE);

    if (optParse(options, argc, argv) == -1) return -1;

    verbose = optIsSet(options, "verbose");

    name = mx_get_name(optArg(options, "name"));
    port = mx_get_port(name);

    if ((host = optArg(options, "host")) == NULL) {
        host = "localhost";
    }

    if ((fd = tcpConnect(host, port)) < 0) {
        fprintf(stderr, "Couldn't connect to master at %s:%d\n", host, port);
        return 1;
    }
    else if (verbose) {
        fprintf(stderr, "Connected to master at %s:%d, sending QUIT request.\n", host, port);
    }

    size = astrpack(&str,
            PACK_INT32, MX_QUIT_REQUEST,
            PACK_INT32, 0,
            PACK_INT32, 0,
            END);

    write(fd, str, size);

    FD_SET(fd, &rfds);

    tv.tv_sec  = 5;
    tv.tv_usec = 0;

    if ((r = select(fd + 1, &rfds, NULL, NULL, &tv)) == 0) {
        fprintf(stderr, "Timed out waiting for master to exit.\n");

        return 1;
    }
    else if ((r = read(fd, str, size)) > 0) {
        fprintf(stderr, "Uhm... master replied to QUIT request?!\n");

        return 1;
    }
    else if (verbose) {
        fprintf(stderr, "Master has exited.\n");

        return 0;
    }
}

/* === The name subcommand === */

static int cmd_name(int argc, char *argv[])
{
    const char *name;

    Options *options = optCreate();

    optAdd(options, "name", 'n', ARG_REQUIRED);

    if (optParse(options, argc, argv) == -1) return -1;

    name = mx_get_name(optArg(options, "name"));

    fprintf(stdout, "%s\n", name);

    return 0;
}

/* === The port subcommand === */

static int cmd_port(int argc, char *argv[])
{
    const char *name;
    uint16_t port;

    Options *options = optCreate();

    optAdd(options, "name", 'n', ARG_REQUIRED);

    if (optParse(options, argc, argv) == -1) return -1;

    name = mx_get_name(optArg(options, "name"));
    port = mx_get_port(name);

    fprintf(stdout, "%d\n", port);

    return 0;
}

int main(int argc, char *argv[])
{
    int r = 0;

    char *command;

    if (argc == 1) {
        usage(stderr, argv[0], 0);
    }

    command = argv[1];

    if (strcmp(command, "help") == 0) {
        usage(stderr, argv[0], 0);
    }
    else if (strcmp(command, "master") == 0) {
        r = cmd_master(argc - 1, argv + 1);
    }
    else if (strcmp(command, "quit") == 0) {
        r = cmd_quit(argc - 1, argv + 1);
    }
    else if (strcmp(command, "name") == 0) {
        r = cmd_name(argc - 1, argv + 1);
    }
    else if (strcmp(command, "port") == 0) {
        r = cmd_port(argc - 1, argv + 1);
    }
    else {
        fprintf(stderr, "Unknown subcommand \"%s\"\n", command);
        r = -1;
    }

    if (r < 0) {
        usage(stderr, argv[0], r);
    }

    return r;
}
