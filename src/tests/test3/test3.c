/*
 * test3.c: Description
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: test3.c 123 2013-10-17 18:41:53Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdlib.h>
#include <stdio.h>

#include <libjvs/log.h>

#include "libmx.h"

Logger *logger;

void on_message(MX *mx, int fd, MX_Type msg_type, MX_Version version,
        MX_Size size, char *payload, void *udata)
{
    const char *msg_name = mxMessageName(mx, msg_type);

    logWrite(logger, "Message type %d (%s) on fd %d\n", msg_type, msg_name, fd);

    free(payload);
}

void on_new_component(MX *mx, int fd, const char *name, void *udata)
{
    logWrite(logger, "New component %s on fd %d.\n", name, fd);
}

void on_end_component(MX *mx, int fd, const char *name, void *udata)
{
    logWrite(logger, "End of component %s on fd %d.\n", name, fd);
}

void on_new_subscriber(MX *mx, MX_Type type, int fd, void *udata)
{
    const char *msg_name = mxMessageName(mx, type);
    const char *comp_name = mxComponentName(mx, fd);

    logWrite(logger, "Component %s on fd %d subscribes to message type %d (%s)\n",
            comp_name, fd, type, msg_name);
}

void on_end_subscriber(MX *mx, MX_Type type, int fd, void *udata)
{
    const char *msg_name = mxMessageName(mx, type);
    const char *comp_name = mxComponentName(mx, fd);

    logWrite(logger, "Component %s on fd %d cancels subscription to message type %d (%s)\n",
            comp_name, fd, type, msg_name);
}

void on_new_publisher(MX *mx, MX_Type type, int fd, void *udata)
{
    const char *msg_name = mxMessageName(mx, type);
    const char *comp_name = mxComponentName(mx, fd);

    logWrite(logger, "Component %s on fd %d publishes message type %d (%s)\n",
            comp_name, fd, type, msg_name);
}

void on_end_publisher(MX *mx, MX_Type type, int fd, void *udata)
{
    const char *msg_name = mxMessageName(mx, type);
    const char *comp_name = mxComponentName(mx, fd);

    logWrite(logger, "Component %s on fd %d withdrew publication of message type %d (%s)\n",
            comp_name, fd, type, msg_name);
}

void on_new_message(MX *mx, const char *name, MX_Type type, void *udata)
{
    logWrite(logger, "New message type %d (%s).\n", type, name);

    mxOnNewSubscriber(mx, type, on_new_subscriber, NULL);
    mxOnEndSubscriber(mx, type, on_end_subscriber, NULL);

    mxOnNewPublisher(mx, type, on_new_publisher, NULL);
    mxOnEndPublisher(mx, type, on_end_publisher, NULL);
}

int main(int argc, char *argv[])
{
    int i, sub = argv[1][0] - '0';
    char my_name[6] = "Testx";
    MX_Type msg[4];

    my_name[4] = argv[1][0];

    MX *mx = mxConnect(NULL, "localhost", my_name);

    logger = logCreate();
    logToFP(logger, stdout);
#if 0
    logWithDate(logger);
    logWithTime(logger, 6);
#endif
    logWithString(logger, my_name);

    mxOnNewComponent(mx, on_new_component, NULL);
    mxOnEndComponent(mx, on_end_component, NULL);
    mxOnNewMessage(mx, on_new_message, NULL);

    for (i = 1; i <= sub; i++) {
        char msg_name[5];

        snprintf(msg_name, sizeof(msg_name), "Msg%d", i);

        msg[i] = mxRegister(mx, msg_name);

        on_new_message(mx, msg_name, msg[i], NULL);

        if (i == sub) {
            mxPublish(mx, msg[i]);
        }
        else {
            mxSubscribe(mx, msg[i], on_message, NULL);
        }
    }

    mxRun(mx);

    return 0;
}
