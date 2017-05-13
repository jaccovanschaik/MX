/*
 * test2.c: Description
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: test2.c 108 2013-10-14 18:31:33Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libjvs/ns.h>
#include <libjvs/utils.h>

#include "libmx.h"
#include "private.h"

static int master_fd[2], master_port;
static int listen_fd, listen_port;

static char *expected_msg = NULL;
static int   expected_size = 0;
static int   expected_fd = -1;
static double wait_until = 0;
static int step;

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

/*
 * Close down when the master disconnect from me.
 */
static void test_on_disconnect(NS *ns, int fd, void *udata)
{
    if (fd == master_fd[0]) {
        nsClose(ns);
    }
}

/*
 * When an expected reply doesn't arrive in time, simply kill the master and exit.
 */
static void test_on_timeout(NS *ns, double t, void *udata)
{
    fprintf(stderr, "Timeout!\n");

    exit(1);
}

/*
 * Expect a message to arrive on <fd> via <ns>, within <timeout>, with the contents as described by
 * the PACK_* arguments following <timeout>. If it doesn't arrive before then, the test_on_timeout
 * function is called.
 */
static void test_expect(NS *ns, int fd, double timeout, ...)
{
    va_list ap;

    expected_fd = fd;

    wait_until = nowd() + timeout;

    nsOnTime(ns, wait_until, test_on_timeout, NULL);

    if (expected_msg != NULL) free(expected_msg);

    va_start(ap, timeout);
    expected_size = vastrpack(&expected_msg, ap);
    va_end(ap);
}

/*
 * Do a step in the test sequence.
 */
static void test_step(NS *ns, double t, void *udata)
{
    switch(step) {
    case 0:
        /* Contact the master. */

        master_fd[0] = nsConnect(ns, "localhost", master_port);

        if (master_fd[0] < 0) {
            fprintf(stderr, "Could not connect to master component on port %d\n", master_port);
            exit(1);
        }

        /* Send an MX_HELLO_REQUEST. */

        send_message(ns, master_fd[0], MX_HELLO_REQUEST, 0,
                PACK_STRING, "Test1",           /* My name */
                PACK_INT16,  listen_port,       /* My listen port */
                END);

        /* I expect an MX_HELLO_REPLY with my component ID, which I expect to be 1. */

        test_expect(ns, master_fd[0], 5,
                PACK_INT32, MX_HELLO_REPLY,
                PACK_INT32, 0,
                PACK_INT32, 2,
                PACK_INT16, 1,                  /* My component ID. */
                END);

        break;
    case 1:
        /* Register a message with name "Message1". */

        send_message(ns, master_fd[0], MX_REGISTER_REQUEST, 0,
                PACK_STRING,    "Message1",
                END);

        /* I expect an MX_REGISTER_REPLY with the first available message type. */

        test_expect(ns, master_fd[0], 5,
                PACK_INT32, MX_REGISTER_REPLY,
                PACK_INT32, 0,
                PACK_INT32, 4,
                PACK_INT32, NUM_MX_MESSAGES,    /* First available message type. */
                END);

        break;
    case 2:
        /* Now make a second connection to the master and pretend I'm a new component. */

        master_fd[1] = nsConnect(ns, "localhost", master_port);

        if (master_fd[1] < 0) {
            fprintf(stderr, "Could not connect to master component on port %d\n", master_port);
            exit(1);
        }

        send_message(ns, master_fd[1], MX_HELLO_REQUEST, 0,
                PACK_STRING, "Test2",           /* My second name */
                PACK_INT16,  listen_port,       /* My listen port */
                END);

        /* I expect an MX_HELLO_REPLY for component Test2, and an MX_HELLO_REPORT for each component
         * that is already connected (which is just Test1 in this case). */

        test_expect(ns, master_fd[1], 5,
                PACK_INT32, MX_HELLO_REPLY,     /* First, a hello reply to me. */
                PACK_INT32, 0,                  /* Version */
                PACK_INT32, 2,                  /* Payload size */
                PACK_INT16, 2,                  /* My component ID. */
                PACK_INT32, MX_HELLO_REPORT,    /* Then a hello report for Test1 */
                PACK_INT32, 0,                  /* Version */
                PACK_INT32, 2 + 4 + 5 + 4 + 9 + 2,  /* Payload size. */
                PACK_INT16, 1,                  /* His component id */
                PACK_STRING,"Test1",            /* His name */
                PACK_STRING,"localhost",        /* His host */
                PACK_INT16, listen_port,        /* His (and also my) port */
                END);

        break;
    default:
        /* Close down. */

        exit(0);

        break;
    }
}

/*
 * Got some incoming data on <fd>. Handle this.
 */
static void test_on_socket(NS *ns, int fd, const char *buffer, int size, void *udata)
{
    if (size < expected_size) return;

    nsDropTime(ns, wait_until, test_on_timeout);

    if (fd == expected_fd && memcmp(buffer, expected_msg, expected_size) == 0) {
        step++;

P       fprintf(stderr, "Got expected reply, continuing with step %d\n", step);

        nsOnTime(ns, nowd() + 0.1, test_step, NULL);
        wait_until = nowd() + 5;
        nsOnTime(ns, wait_until, test_on_timeout, NULL);

        nsDiscard(ns, fd, expected_size);
    }
    else if (fd == expected_fd && size >= expected_size) {
        fprintf(stderr, "Unexpected reply.\nExpected:\n");
        ihexdump(stderr, 1, expected_msg, expected_size);
        fprintf(stderr, "Received:\n");
        ihexdump(stderr, 1, buffer, size);
    }
}

/*
 * Run the "test" subcommand.
 */
int main(int argc, char *argv[])
{
    NS *ns;
    int r;

    ns = nsCreate();

    listen_fd   = nsListen(ns, NULL, 0);
    listen_port = netLocalPort(listen_fd);
    master_port = mx_get_port(mx_get_name(NULL));

    nsOnSocket(ns, test_on_socket, NULL);
    nsOnDisconnect(ns, test_on_disconnect, NULL);

    step = 0;

    nsOnTime(ns, nowd() + 0.1, test_step, NULL);

    r = nsRun(ns);

    nsDestroy(ns);

    return r;
}
