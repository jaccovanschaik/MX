/*
 * producer.c: Message producer for test3.
 *
 * Copyright:	(c) 2014-2025 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: producer.c 330 2016-07-21 11:20:35Z jacco $
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

static uint32_t test_msg;
static uint32_t msg_number = 0;

static MX_Timer *timer;

void on_new_subscriber(MX *mx, int fd, uint32_t type, void *udata)
{
    uint32_t i;

    for (i = 0; i < msg_number; i++) {
        mxPackAndSend(mx, fd, test_msg, 0,
                PACK_INT32, i,
                END);
    }
}

void on_time(MX *mx, MX_Timer *timer, double t, void *udata)
{
    printf("Producer: broadcasting msg %d\n", msg_number);

    if (msg_number < 10) {
        mxPackAndBroadcast(mx, test_msg, 0,
                PACK_INT32, msg_number,
                END);

        msg_number++;

        mxAdjustTimer(mx, timer, t + 1);
    }
    else {
        mxShutdown(mx);
    }
}

int main(int argc, char *argv[])
{
    int r;

    MX *mx = mxClient("localhost", NULL, "Producer");

    fprintf(stderr, "Producer starting.\n");

    if (mx == NULL) {
        fprintf(stderr, "mxClient failed.\n");
        return 1;
    }

    test_msg = mxRegister(mx, "Test");

    timer = mxCreateTimer(mx, mxNow() + 1, on_time, NULL);

    mxOnNewSubscriber(mx, test_msg, on_new_subscriber, NULL);

    r = mxRun(mx);

    if (r != 0) {
        fputs(mxError(), stderr);
    }

    mxDestroy(mx);

    return r;
}
