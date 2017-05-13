/*
 * MX Test object program.
 *
 * This program receives messages from the mx_test_driver program. It writes log output to standard
 * output. This output is compared to an earlier, validated output log to check that the program
 * still handles the received messages correctly.
 *
 * Author:	Jacco van Schaik (jacco.van.schaik@dnw.aero)
 * Copyright:	(c) 2013 DNW German-Dutch Windtunnels
 * Created:	2013-10-09
 * Version:	$Id: mx_test_object.c 108 2013-10-14 18:31:33Z jacco $
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <libjvs/tcp.h>
#include <libjvs/log.h>
#include <libjvs/utils.h>

#include "libmx.h"

MX_Type ctrl_msg;
MX_Type ping_msg;
MX_Type echo_msg;

Logger *logger;

void handle_ctrl(MX *mx, int fd, MX_Type type, MX_Version version,
        MX_Size size, char *payload, void *udata)
{
    char *str;

    strunpack(payload, size,
            PACK_STRING, &str,
            END);

    free(payload);

    logWrite(logger, "fd = %d, type = %d, version = %d, size = %d, payload = \"%s\"\n",
            fd, type, version, size, str);

    if (strcmp(str, "Ping me?") == 0) {
        char *msg = "Ping!";

        mxPack(mx, fd, ping_msg, 0, PACK_STRING, msg, END);

        logWrite(logger, "sent \"%s\", awaiting echo...\n", msg);

        mxAwait(mx, fd, echo_msg, &version, &size, &payload, 5);

        strunpack(payload, size,
                PACK_STRING, &str,
                END);

        free(payload);

        logWrite(logger, "received \"%s\"\n", str);

        free(str);
    }
    else if (strcmp(str, "Quit") == 0) {
        logWrite(logger, "calling mxClose()\n");

        mxClose(mx);
    }
}

void on_data(MX *mx, int fd, void *udata)
{
    char data[9000];

    int n = read(fd, data, sizeof(data));

    if (n == 0) {
        logWrite(logger, "end of file on fd %d.\n", fd);
        mxDropData(mx, fd);
    }
    else {
        logWrite(logger, "read %d bytes from fd %d: %.*s\n", n, fd, n, data);
    }
}

void on_connection_request(MX *mx, int fd, void *udata)
{
    logWrite(logger, "connection request on fd %d\n", fd);

    fd = tcpAccept(fd);

    logWrite(logger, "accepted connection, new fd = %d\n", fd);

    mxOnData(mx, fd, on_data, NULL);
}

void on_new_ctrl_publisher(MX *mx, MX_Type type, int fd, void *udata)
{
    logWrite(logger, "type = %d (%s), fd = %d (%s)\n",
            type, mxMessageName(mx, type), fd, mxComponentName(mx, fd));

    logWrite(logger, "publisher count = %d\n", mxPublisherCount(mx, type));
}

void on_new_ping_subscriber(MX *mx, MX_Type type, int fd, void *udata)
{
    logWrite(logger, "type = %d (%s), fd = %d (%s)\n",
            type, mxMessageName(mx, type), fd, mxComponentName(mx, fd));

    logWrite(logger, "subscriber count = %d\n", mxSubscriberCount(mx, type));
}

void on_end_ctrl_publisher(MX *mx, MX_Type type, int fd, void *udata)
{
    logWrite(logger, "type = %d (%s), fd = %d (%s)\n",
            type, mxMessageName(mx, type), fd, mxComponentName(mx, fd));

    logWrite(logger, "publisher count = %d\n", mxPublisherCount(mx, type));
}

void on_end_ping_subscriber(MX *mx, MX_Type type, int fd, void *udata)
{
    logWrite(logger, "type = %d (%s), fd = %d (%s)\n",
            type, mxMessageName(mx, type), fd, mxComponentName(mx, fd));

    logWrite(logger, "subscriber count = %d\n", mxSubscriberCount(mx, type));
}

void on_new_component(MX *mx, int fd, const char *name, void *udata)
{
    logWrite(logger, "fd = %d, name = \"%s\"\n", fd, name);
}

void on_end_component(MX *mx, int fd, const char *name, void *udata)
{
    logWrite(logger, "fd = %d, name = \"%s\"\n", fd, name);
}

int main(int argc, char *argv[])
{
    int r;

    int listen_port;
    int listen_fd;

    MX *mx;

    if (argc == 1) {
        fprintf(stderr, "Usage: %s <listen_port>\n", argv[0]);
        return 0;
    }

    mx = mxConnect(NULL, "localhost", argv[0]);

    if (mx == NULL) {
        fprintf(stderr, "mxConnect failed\n");
        exit(1);
    }

    logger = logCreate();
    logToFP(logger, stdout);
    logWithFunction(logger);
    logWithString(logger, ":");

    ctrl_msg = mxRegister(mx, "Control");
    ping_msg = mxRegister(mx, "Ping");
    echo_msg = mxRegister(mx, "Echo");

    logWrite(logger, "ctrl_msg = %d\n", ctrl_msg);
    logWrite(logger, "ping_msg = %d\n", ping_msg);
    logWrite(logger, "echo_msg = %d\n", echo_msg);

    mxSubscribe(mx, ctrl_msg, handle_ctrl, NULL);
    mxPublish(mx, ping_msg);

    listen_port = atoi(argv[1]);
    listen_fd   = tcpListen(NULL, listen_port);

    mxOnData(mx, listen_fd, on_connection_request, NULL);

    mxOnNewComponent(mx, on_new_component, NULL);
    mxOnEndComponent(mx, on_end_component, NULL);

    mxOnNewPublisher(mx, ctrl_msg, on_new_ctrl_publisher, NULL);
    mxOnEndPublisher(mx, ctrl_msg, on_end_ctrl_publisher, NULL);

    mxOnNewSubscriber(mx, ping_msg, on_new_ping_subscriber, NULL);
    mxOnEndSubscriber(mx, ping_msg, on_end_ping_subscriber, NULL);

    r = mxRun(mx);

    mxDestroy(mx);

    return r;
}
