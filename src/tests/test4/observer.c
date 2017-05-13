/*
 * observer.c: Observe and log the interactions between the components.
 *
 * Copyright:	(c) 2014 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: observer.c 318 2016-07-18 19:52:08Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <libjvs/utils.h>
#include <libjvs/debug.h>

#include "libmx.h"

static uint32_t ping_msg, echo_msg;
static int ping_count = 0, echo_count = 0;

void handler(MX *mx, int fd, uint32_t type, uint32_t version,
        char *payload, uint32_t size, void *udata)
{
    uint32_t counter;

    strunpack(payload, size,
            PACK_INT32, &counter,
            END);

    free(payload);

    printf("Observer: received %s %d.\n",
            mxMessageName(mx, type), counter);

    if (type == ping_msg)
        ping_count++;
    else if (type == echo_msg)
        echo_count++;
}

void on_new_subscriber(MX *mx, int fd, uint32_t type, void *udata)
{
P   fprintf(stderr, "on_new_subscriber: fd = %d, type = %u\n", fd, type);

    printf("Observer: %s subscribes to %s messages.\n",
            mxComponentName(mx, fd), mxMessageName(mx, type));
}

void on_end_subscriber(MX *mx, int fd, uint32_t type, void *udata)
{
P   fprintf(stderr, "on_end_subscriber: fd = %d, type = %u\n", fd, type);

    printf("Observer: %s cancels subscription to %s messages.\n",
            mxComponentName(mx, fd), mxMessageName(mx, type));
}

void on_new_component(MX *mx, int fd, const char *name, void *udata)
{
P   fprintf(stderr, "on_new_component: fd = %d, name = %s\n", fd, name);

    printf("Observer: new component %s.\n", name);
}

void on_end_component(MX *mx, int fd, const char *name, void *udata)
{
P   fprintf(stderr, "on_end_component: fd = %d, name = %s\n", fd, name);

    printf("Observer: end of component %s.\n", name);
}

void on_new_message(MX *mx, uint32_t type, const char *name, void *udata)
{
    printf("Observer: new message %s, type = %d.\n", name, type);

    mxOnNewSubscriber(mx, type, on_new_subscriber, NULL);
    mxOnEndSubscriber(mx, type, on_end_subscriber, NULL);
}

int main(int argc, char *argv[])
{
    int r;

    MX *mx = mxClient("localhost", NULL, "Observer");

    if (mx == NULL) {
        printf("mxClient failed: %s\n", mxError());
        return 1;
    }

    ping_msg = mxRegister(mx, "Ping");
    echo_msg = mxRegister(mx, "Echo");

    printf("Observer: ping_msg = %d.\n", ping_msg);
    printf("Observer: echo_msg = %d.\n", echo_msg);

    mxSubscribe(mx, ping_msg, handler, NULL);
    mxSubscribe(mx, echo_msg, handler, NULL);

    mxOnNewComponent(mx, on_new_component, NULL);
    mxOnEndComponent(mx, on_end_component, NULL);

    mxOnNewMessage(mx, on_new_message, NULL);

    r = mxRun(mx);

    printf("Observer: mxRun returned %d.\n", r);

    if (r != 0) {
        fputs(mxError(), stderr);
    }

    mxDestroy(mx);

    printf("Observer: received %d pings and %d echos.\n",
            ping_count, echo_count);

    return r;
}
