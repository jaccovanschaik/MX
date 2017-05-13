/*
 * consumer.c: Message consumer for test3.
 *
 * Copyright:	(c) 2014 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: consumer.c 317 2016-07-18 18:58:59Z jacco $
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
static int my_number;
static int received[10] = { 0 };

void msg_handler(MX *mx, int fd, uint32_t type, uint32_t version,
        char *payload, uint32_t size, void *udata)
{
    uint32_t msg_number;

    strunpack(payload, size,
            PACK_INT32, &msg_number,
            END);

    free(payload);

    received[msg_number] = 1;
}

int main(int argc, char *argv[])
{
    int r, i, n = sizeof(received) / sizeof(received[0]);
    char my_name[10];

    my_number = atoi(argv[1]);

    snprintf(my_name, sizeof(my_name), "Consumer%d", my_number);

    MX *mx = mxClient("localhost", NULL, my_name);

    fprintf(stderr, "Consumer %d starting.\n", my_number);

    if (mx == NULL) {
        fprintf(stderr, "mxClient failed.\n");
        return 1;
    }

    test_msg = mxRegister(mx, "Test");

    mxSubscribe(mx, test_msg, msg_handler, NULL);

    r = mxRun(mx);

    if (r != 0) {
        fputs(mxError(), stderr);
    }

    mxDestroy(mx);

    printf("%d:", my_number);

    for (i = 0; i < n; i++) {
        if (received[i])
            printf(" %d", i);
        else
            printf("  ");
    }

    printf("\n");

    return r;
}
