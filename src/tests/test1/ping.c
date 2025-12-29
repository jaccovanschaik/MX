/*
 * ping.c: Broadcasts "Ping" messages at regular intervals.
 *
 * Copyright:	(c) 2014-2025 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: ping.c 461 2022-01-31 09:02:30Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <stdint.h>

#include <libjvs/utils.h>
#include <libjvs/debug.h>

#include "libmx.h"

static uint32_t ping_msg;

MX_Timer *timer;

void on_time(MX *mx, MX_Timer *timer, double t, void *udata)
{
    static int count = 0;

    count++;

    if (count <= 5) {
        fprintf(stderr, "Ping: broadcasting ping %d.\n", count);

        mxPackAndBroadcast(mx, ping_msg, 0,
                PACK_INT32, count,
                END);

        mxAdjustTimer(mx, timer, t + 1);
    }
    else {
        fprintf(stderr, "Ping: shutting down.\n");

        mxShutdown(mx);

        return;
    }
}

int main(int argc, char *argv[])
{
    int r;

    MX *mx = mxClient("localhost", NULL, "Ping");

    if (mx == NULL) {
        fprintf(stderr, "mxClient failed: %s\n", mxError());
        return 1;
    }

    ping_msg = mxRegister(mx, "Ping");

    fprintf(stderr, "Ping: ping_msg = %d.\n", ping_msg);

    timer = mxCreateTimer(mx, mxNow() + 1, on_time, NULL);

    r = mxRun(mx);

    fprintf(stderr, "Ping: mxRun returned %d.\n", r);

    if (r != 0) {
        fputs(mxError(), stderr);
    }

    mxDestroy(mx);

    return r;
}
