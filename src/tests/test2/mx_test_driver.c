/*
 * MX Test driver.
 *
 * This program sends messages to the mx_test_object program, which writes log output to standard
 * output. This output is then compared to a previously generated baseline to check if it is still
 * correct.
 *
 * Author:	Jacco van Schaik (jacco.van.schaik@dnw.aero)
 * Copyright:	(c) 2013 DNW German-Dutch Windtunnels
 * Created:	2013-10-09
 * Version:	$Id: mx_test_driver.c 108 2013-10-14 18:31:33Z jacco $
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <libjvs/tcp.h>
#include <libjvs/utils.h>

#include "libmx.h"

int data_port;
int data_fd = -1;

MX_Type ctrl_msg;
MX_Type ping_msg;
MX_Type echo_msg;

int step = 0;

void close_down(MX *mx)
{
    if (data_fd >= 0) {
        close(data_fd);
    }

    mxClose(mx);
}

void handle_ping(MX *mx, int fd, MX_Type type, MX_Version version,
        MX_Size size, char *payload, void *udata)
{
    char *str;

    strunpack(payload, size,
            PACK_STRING,    &str,
            END);

    free(payload);

    if (strcmp(str, "Ping!") == 0) {
        mxPack(mx, fd, echo_msg, 0, PACK_STRING, "Echo!", END);
    }

    free(str);
}

void test_step(MX *mx, double t, void *udata)
{
    step++;

    switch(step) {
    case 1:
        ctrl_msg = mxRegister(mx, "Control");
        ping_msg = mxRegister(mx, "Ping");
        echo_msg = mxRegister(mx, "Echo");
        break;
    case 2:
        mxPublish(mx, ctrl_msg);
        mxSubscribe(mx, ping_msg, handle_ping, NULL);
        break;
    case 3:
        data_fd = tcpConnect("localhost", data_port);
        if (data_fd < 0) close_down(mx);
        break;
    case 4:
        write(data_fd, "Data test.", 10);
        break;
    case 5:
        close(data_fd);
        break;
    case 6:
        mxBroadcast(mx, ctrl_msg, 0,
                PACK_STRING,    "Ping me?",
                END);
        break;
    default:
        mxBroadcast(mx, ctrl_msg, 0,
                PACK_STRING,    "Quit",
                END);
        break;
    }

    mxOnTime(mx, t + 0.1, test_step, udata);
}

int main(int argc, char *argv[])
{
    int r;
    MX *mx;

    if (argc == 1) {
        fprintf(stderr, "Usage: %s <listen_port>\n", argv[0]);
        return 0;
    }

    data_port = atoi(argv[1]);

    mx = mxConnect(NULL, "localhost", argv[0]);

    mxOnTime(mx, nowd() + 0.11, test_step, NULL);

    r = mxRun(mx);

    mxDestroy(mx);

    return r;
}
