/*
 * mx.c: The "mx" executable.
 *
 * Copyright:	(c) 2014 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: mx.c 441 2019-07-20 19:32:45Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

/* Test */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libjvs/options.h>
#include <libjvs/utils.h>
#include <libjvs/tcp.h>

#include "types.h"
#include "msg.h"
#include "libmx.h"
#include "version.h"

/*
 * Show usage for the mx command.
 */
static void mx_usage(const char *argv0)
{
    fprintf(stderr, "Usage: %s <command> [ <options> ]\n\n", argv0);

    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  help     Show help\n");
    fprintf(stderr, "  master   Run a master component\n");
    fprintf(stderr, "  name     Print the effective MX name\n");
    fprintf(stderr, "  host     Print the effective MX host\n");
    fprintf(stderr, "  port     Print the effective MX port\n");
    fprintf(stderr, "  list     Show a list of participating components\n");
    fprintf(stderr, "  quit     Ask the master component to exit\n");
    fprintf(stderr, "  version  Show the current version of MX\n\n");
    fprintf(stderr, "Use \"%s help <command>\" to get help on a specific command.\n", argv0);
}

/*
 * Start an MX master.
 */
static int mx_master(int argc, char *argv[])
{
    MX *mx;
    int r;
    const char *name;

    Options *options = optCreate();

    optAdd(options, "mx-name", 'n', ARG_REQUIRED);
    optAdd(options, "daemon", 'd', ARG_NONE);

    if (optParse(options, argc, argv) == -1) return 1;

    name = mxEffectiveName(optArg(options, "mx-name", NULL));

    mx = mxCreateMaster(name, NULL);

    if (mx == NULL) {
        fputs(mxError(), stderr);
        return 1;
    }

    if (!optIsSet(options, "daemon")) {
        fprintf(stderr, "Master listening on port %d for mx \"%s\"\n",
                mxEffectivePort(name), name);

        if ((r = mxBegin(mx)) != 0) {
            fprintf(stderr, "mxBegin returned %d: %s", r, mxError());
        }
        else if ((r = mxRun(mx)) != 0) {
            fprintf(stderr, "mxRun returned %d: %s", r, mxError());
        }
    }
    else if (daemon(0, 1) == 0) {
        if ((r = mxBegin(mx)) != 0) {
            fprintf(stderr, "mxBegin returned %d: %s", r, mxError());
        }
        else if ((r = mxRun(mx)) != 0) {
            fprintf(stderr, "mxRun returned %d: %s", r, mxError());
        }
    }
    else {
        fprintf(stderr, "daemon() failed\n");

        r = 1;
    }

    optDestroy(options);

    mxDestroy(mx);

    return r;
}

/*
 * Execute the "mx name" command.
 */
static int mx_name(int argc, char *argv[])
{
    printf("%s\n", mxEffectiveName(NULL));

    return 0;
}

/*
 * Execute the "mx host" command.
 */
static int mx_host(int argc, char *argv[])
{
    printf("%s\n", mxEffectiveHost(NULL));

    return 0;
}

/*
 * Execute the "mx port" command.
 */
static int mx_port(int argc, char *argv[])
{
    int port;

    Options *options = optCreate();

    optAdd(options, "mx-name", 'n', ARG_REQUIRED);

    if (optParse(options, argc, argv) == -1) {
        return 1;
    }
    else if (optIsSet(options, "mx-name")) {
        port = mxEffectivePort(optArg(options, "mx-name", NULL));
    }
    else {
        port = mxEffectivePort(NULL);
    }

    printf("%d\n", port);

    return 0;
}

/*
 * Execute the "mx quit" command.
 */
static int mx_quit(int argc, char *argv[])
{
    const char *host, *name;
    uint16_t port;
    int fd, size, verbose;
    struct timeval tv;
    fd_set rfds = { };

    char *msg;

    Options *options = optCreate();

    optAdd(options, "mx-name", 'n', ARG_REQUIRED);
    optAdd(options, "mx-host", 'h', ARG_REQUIRED);
    optAdd(options, "verbose", 'v', ARG_NONE);

    if (optParse(options, argc, argv) == -1) {
        return 1;
    }

    verbose = optIsSet(options, "verbose");

    host = mxEffectiveHost(optArg(options, "mx-host", NULL));
    name = mxEffectiveName(optArg(options, "mx-name", NULL));
    port = mxEffectivePort(name);

    if (verbose) {
        fprintf(stderr, "Connecting to master for \"%s\" at %s:%d... ",
                name, host, port);
    }

    fd = tcpConnect(host, port);

    if (fd == -1) {
        if (verbose) {
            fprintf(stderr, "failed.\n");
        }
        else {
            fprintf(stderr,
                    "Couldn't connect to master for \"%s\" at %s:%d.\n",
                    name, host, port);
        }
        return 1;
    }
    else if (verbose) {
        fprintf(stderr, "done.\n");
    }

    if (verbose) {
        fprintf(stderr, "Sending quit request.\n");
    }

    size = astrpack(&msg,
            PACK_INT32, MX_MT_QUIT_REQUEST,
            PACK_INT32, 0,
            PACK_INT32, 0,
            END);

    tcpWrite(fd, msg, size);

    FD_SET(fd, &rfds);

    tv.tv_sec  = 5;
    tv.tv_usec = 0;

    if (verbose) {
        fprintf(stderr, "Waiting for master to exit... ");
    }

    if (select(fd + 1, &rfds, NULL, NULL, &tv) == 0) {
        if (verbose) {
            fprintf(stderr, "timeout!\n");
        }
        else {
            fprintf(stderr, "Timed out waiting for master to exit.\n");
        }

        return 1;
    }
    else if (read(fd, msg, size) > 0) {
        fprintf(stderr, "master replied?!\n");

        return 1;
    }
    else if (verbose) {
        fprintf(stderr, "done.\n");
    }

    optDestroy(options);

    free(msg);

    return 0;
}

/*
 * Timeout routine for the "mx list" command.
 */
static void mx_list_on_timeout(MX *mx, uint32_t id, double t, void *udata)
{
    int i, verbosity;
    const char *arg;

    Options *options = udata;

    if (!optIsSet(options, "verbose")) {
        verbosity = 0;
    }
    else if ((arg = optArg(options, "verbose", NULL)) == NULL) {
        verbosity = 1;
    }
    else if ((verbosity = arg[0] - '0') < 0 || verbosity > 2) {
        fprintf(stderr, "Verbosity level out of bounds (0 - 2)\n");
        exit(1);
    }

    /* OK, print all the components in <mx>. */

    for (i = 0; i < paCount(&mx->components); i++) {
        MX_Component *comp = paGet(&mx->components, i);

        if (comp == NULL) continue;

        fprintf(stdout, "%s (%s:%d)\n",
                comp->name, comp->host, comp->port);

        if (verbosity > 0) {
            MX_Subscription *sub;

            for (sub = mlHead(&comp->subscriptions); sub;
                 sub = mlNext(&comp->subscriptions, sub)) {
                if (verbosity == 2 || sub->msg->msg_type >= NUM_MX_MESSAGES) {
                    fprintf(stdout, "\t%d (%s)\n",
                            sub->msg->msg_type, sub->msg->msg_name);
                }
            }
        }
    }

    mxShutdown(mx);
}

/*
 * Execute the "mx list" command.
 */
static int mx_list(const char *argv0, int argc, char *argv[])
{
    Options *options = optCreate();

    int next_arg;

    optAdd(options, "mx-name", 'n', ARG_REQUIRED);
    optAdd(options, "mx-host", 'h', ARG_REQUIRED);
    optAdd(options, "verbose", 'v', ARG_OPTIONAL);

    if ((next_arg = optParse(options, argc, argv)) == -1) {
        fprintf(stderr, "optParse returned -1.\n");
        return 1;
    }
    else if (next_arg != argc) {
        fprintf(stderr, "Unexpected argument \"%s\"\n\n", argv[next_arg]);
        mx_usage(argv0);
        exit(1);
    }

    const char *host = mxEffectiveHost(optArg(options, "mx-host", NULL));
    const char *name = mxEffectiveName(optArg(options, "mx-name", NULL));

    /*
     * To get the participating components we'll create a new client, have it
     * connect normally to the master (which means it'll receive HelloReport
     * messages for all components) and print the reported components after a 1
     * second timeout.
     */

    MX *mx = mxClient(host, name, "mx-list");

    if (mx == NULL) {
        fputs(mxError(), stderr);
        return 1;
    }

    /* Call mx_list_on_timeout after 1 second... */

    mxCreateTimer(mx, 0, mxNow() + 1, mx_list_on_timeout, options);

    /* mxRun won't return until mx_list_on_timeout calls mxShutdown. */

    int r = mxRun(mx);

    optDestroy(options);
    mxDestroy(mx);

    return r;
}

/*
 * Execute the "mx help" command.
 */
static int mx_help(const char *argv0, int argc, char *argv[])
{
    if (argc == 1) {
        mx_usage(argv0);
    }
    else if (strcmp(argv[1], "master") == 0) {
        fprintf(stderr,
                "%s master [ <options> ]\n\tStarts an MX master.\n\n", argv0);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "\t-n, --mx-name <name>\tUse this MX name.\n");
        fprintf(stderr, "\t-d, --daemon\t\tRun as a daemon.\n");
    }
    else if (strcmp(argv[1], "name") == 0) {
        fprintf(stderr,
                "%s name\n\tPrints the effective MX name.\n", argv0);
    }
    else if (strcmp(argv[1], "host") == 0) {
        fprintf(stderr,
                "%s host\n\tPrints the effective MX host.\n", argv0);
    }
    else if (strcmp(argv[1], "port") == 0) {
        fprintf(stderr,
                "%s port [ <options> ]\n\tPrints the effective MX port.\n\n", argv0);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "\t-n, --mx-name <name>\tUse this MX name.\n");
    }
    else if (strcmp(argv[1], "list") == 0) {
        fprintf(stderr,
                "%s list [ <options> ]\n\tLists connected components.\n\n", argv0);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "\t-n, --mx-name <name>\tUse this MX name.\n");
        fprintf(stderr, "\t-h, --mx-host <name>\tUse this MX host.\n");
        fprintf(stderr, "\t-v, --verbose[=<level>]\tVerbosity level:\n");
        fprintf(stderr, "\t\tlevel 1: also show subscriptions (default)\n");
        fprintf(stderr, "\t\tlevel 2: also show system subscriptions\n");
    }
    else if (strcmp(argv[1], "quit") == 0) {
        fprintf(stderr,
                "%s quit [ <options> ]\n\tAsks a master component to exit.\n\n", argv0);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "\t-n, --mx-name <name>\tUse this MX name.\n");
        fprintf(stderr, "\t-h, --mx-host <name>\tUse this MX host.\n");
        fprintf(stderr, "\t-v, --verbose\t\tBe verbose.\n");
    }
    else if (strcmp(argv[1], "version") == 0) {
        fprintf(stderr,
                "%s version\n\tPrints the version of the MX software.\n", argv0);
    }
    else if (strcmp(argv[1], "help") == 0) {
        fprintf(stderr,
                "%s help\n\tShows a list of mx subcommands.\n", argv0);
    }
    else {
        fprintf(stderr, "Unknown command \"%s\".\n\n", argv[1]);
        mx_usage(argv0);
    }

    return 0;
}

/*
 * Main.
 */
int main(int argc, char *argv[])
{
    if (argc == 1) {
        mx_usage(argv[0]);
        exit(0);
    }
    else if (strcmp(argv[1], "master") == 0) {
        return mx_master(argc - 1, argv + 1);
    }
    else if (strcmp(argv[1], "name") == 0) {
        return mx_name(argc - 1, argv + 1);
    }
    else if (strcmp(argv[1], "host") == 0) {
        return mx_host(argc - 1, argv + 1);
    }
    else if (strcmp(argv[1], "port") == 0) {
        return mx_port(argc - 1, argv + 1);
    }
    else if (strcmp(argv[1], "list") == 0) {
        return mx_list(argv[0], argc - 1, argv + 1);
    }
    else if (strcmp(argv[1], "quit") == 0) {
        return mx_quit(argc - 1, argv + 1);
    }
    else if (strcmp(argv[1], "help") == 0) {
        return mx_help(argv[0], argc - 1, argv + 1);
    }
    else if (strcmp(argv[1], "version") == 0) {
        printf("%s\n", VERSION);
        return 0;
    }
    else {
        fprintf(stderr, "Unknown command \"%s\".\n\n", argv[1]);
        mx_usage(argv[0]);
        exit(1);
    }

    return 0;
}
