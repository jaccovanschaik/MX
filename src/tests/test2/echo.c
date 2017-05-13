/*
 * echo.c: Subscribes to "Ping" messages, responds with "Echo" messages.
 *
 * Copyright:	(c) 2014 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: echo.c 371 2016-08-23 10:17:47Z jacco $
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

void on_ping(MX *mx, int fd, uint32_t type, uint32_t version,
        char *payload, uint32_t size, void *udata)
{
    uint32_t count;

    strunpack(payload, size,
            PACK_INT32, &count,
            END);

    free(payload);

    fprintf(stderr, "Echo: received ping %d, broadcasting echo.\n", count);

    mxPackAndBroadcast(mx, echo_msg, 0,
            PACK_INT32, count,
            END);

    if (count == 5) {
        mxCancel(mx, ping_msg);
    }
}

int main(int argc, char *argv[])
{
    int r;

    MX *mx = mxClient("localhost", NULL, "Echo");

    if (mx == NULL) {
        fprintf(stderr, "mxClient failed.\n");
        return 1;
    }

    ping_msg = mxRegister(mx, "Ping");
    echo_msg = mxRegister(mx, "Echo");

    fprintf(stderr, "Echo: ping_msg = %d.\n", ping_msg);
    fprintf(stderr, "Echo: echo_msg = %d.\n", echo_msg);

    mxSubscribe(mx, ping_msg, on_ping, NULL);

    r = mxRun(mx);

    fprintf(stderr, "Echo: mxRun returned %d.\n", r);

    if (r != 0) {
        fputs(mxError(), stderr);
    }

    mxDestroy(mx);

    return r;
}
