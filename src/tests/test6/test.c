/* test.c: Description
 *
 * Copyright: (c) 2016 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2016-07-25
 * Version:   $Id: test.c 407 2017-02-11 19:08:42Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <libmx.h>

#include <libjvs/utils.h>

void clear_args(int argc, char *argv[])
{
    int i;

    for (i = 0; i < argc; i++) {
        argv[i] = NULL;
    }
}

int main(int argc, char *argv[])
{
    int errors = 0;

    setenv("USER", "Flipsen", 1);

    if (strcmp(mxEffectiveName(NULL), "Flipsen") != 0) {
        fprintf(stderr, "mxEffectiveName did not return USER env. variable.\n");
        errors++;
    }

    if (mxEffectivePort(NULL) != mxEffectivePort("Flipsen")) {
        fprintf(stderr, "mxEffectivePort did not return port based on USER env. variable.\n");
        errors++;
    }

    setenv("MX_NAME", "Pietersen", 1);

    if (strcmp(mxEffectiveName(NULL), "Pietersen") != 0) {
        fprintf(stderr, "mxEffectiveName did not return MX_NAME env. variable.\n");
        errors++;
    }

    if (mxEffectivePort(NULL) != mxEffectivePort("Pietersen")) {
        fprintf(stderr, "mxEffectivePort did not return port based on MX_NAME env. variable.\n");
        errors++;
    }

    if (strcmp(mxEffectiveName("Jansen"), "Jansen") != 0) {
        fprintf(stderr, "mxEffectiveName did not return argument.\n");
        errors++;
    }

    {
        char *my_argv[] = {
            "test6",
            "-N", "MX_Name",
            "-x", "Foo",
            "-y",
            "--mx-host", "host.test.com",
            "Hieperdepiep",
            "--name", "Test6"
        };
        int my_argc = sizeof(my_argv) / sizeof(my_argv[0]);

        char *mx_host, *mx_name, *my_name;

        int r;

        r = mxOption('N', "mx-name", &my_argc, my_argv, &mx_name);

        make_sure_that(r == 0);
        make_sure_that(my_argc == 9);
        make_sure_that(strcmp(mx_name, "MX_Name") == 0);
        make_sure_that(strcmp(my_argv[0], "test6") == 0);
        make_sure_that(strcmp(my_argv[1], "-x") == 0);
        make_sure_that(strcmp(my_argv[2], "Foo") == 0);
        make_sure_that(strcmp(my_argv[3], "-y") == 0);
        make_sure_that(strcmp(my_argv[4], "--mx-host") == 0);
        make_sure_that(strcmp(my_argv[5], "host.test.com") == 0);
        make_sure_that(strcmp(my_argv[6], "Hieperdepiep") == 0);
        make_sure_that(strcmp(my_argv[7], "--name") == 0);
        make_sure_that(strcmp(my_argv[8], "Test6") == 0);

        r = mxOption('h', "mx-host", &my_argc, my_argv, &mx_host);

        make_sure_that(r == 0);
        make_sure_that(my_argc == 7);
        make_sure_that(strcmp(mx_host, "host.test.com") == 0);
        make_sure_that(strcmp(my_argv[0], "test6") == 0);
        make_sure_that(strcmp(my_argv[1], "-x") == 0);
        make_sure_that(strcmp(my_argv[2], "Foo") == 0);
        make_sure_that(strcmp(my_argv[3], "-y") == 0);
        make_sure_that(strcmp(my_argv[4], "Hieperdepiep") == 0);
        make_sure_that(strcmp(my_argv[5], "--name") == 0);
        make_sure_that(strcmp(my_argv[6], "Test6") == 0);

        r = mxOption('n', "name", &my_argc, my_argv, &my_name);

        make_sure_that(r == 0);
        make_sure_that(my_argc == 5);
        make_sure_that(strcmp(my_name, "Test6") == 0);
        make_sure_that(strcmp(my_argv[0], "test6") == 0);
        make_sure_that(strcmp(my_argv[1], "-x") == 0);
        make_sure_that(strcmp(my_argv[2], "Foo") == 0);
        make_sure_that(strcmp(my_argv[3], "-y") == 0);
        make_sure_that(strcmp(my_argv[4], "Hieperdepiep") == 0);

        clear_args(my_argc, my_argv);
    }

    {
        char *my_argv[] = {
            "test6",
            "-NMX_Name",
            "-x", "Foo",
            "-y",
            "--mx-host=host.test.com",
            "Hieperdepiep",
            "--name=Test6"
        };
        int my_argc = sizeof(my_argv) / sizeof(my_argv[0]);

        char *mx_host, *mx_name, *my_name;

        int r;

        r = mxOption('N', "mx-name", &my_argc, my_argv, &mx_name);

        make_sure_that(r == 0);
        make_sure_that(my_argc == 7);
        make_sure_that(strcmp(mx_name, "MX_Name") == 0);
        make_sure_that(strcmp(my_argv[0], "test6") == 0);
        make_sure_that(strcmp(my_argv[1], "-x") == 0);
        make_sure_that(strcmp(my_argv[2], "Foo") == 0);
        make_sure_that(strcmp(my_argv[3], "-y") == 0);
        make_sure_that(strcmp(my_argv[4], "--mx-host=host.test.com") == 0);
        make_sure_that(strcmp(my_argv[5], "Hieperdepiep") == 0);
        make_sure_that(strcmp(my_argv[6], "--name=Test6") == 0);

        r = mxOption('h', "mx-host", &my_argc, my_argv, &mx_host);

        make_sure_that(r == 0);
        make_sure_that(my_argc == 6);
        make_sure_that(strcmp(mx_name, "MX_Name") == 0);
        make_sure_that(strcmp(my_argv[0], "test6") == 0);
        make_sure_that(strcmp(my_argv[1], "-x") == 0);
        make_sure_that(strcmp(my_argv[2], "Foo") == 0);
        make_sure_that(strcmp(my_argv[3], "-y") == 0);
        make_sure_that(strcmp(my_argv[4], "Hieperdepiep") == 0);
        make_sure_that(strcmp(my_argv[5], "--name=Test6") == 0);

        r = mxOption('n', "name", &my_argc, my_argv, &my_name);

        make_sure_that(r == 0);
        make_sure_that(my_argc == 5);
        make_sure_that(strcmp(mx_name, "MX_Name") == 0);
        make_sure_that(strcmp(my_argv[0], "test6") == 0);
        make_sure_that(strcmp(my_argv[1], "-x") == 0);
        make_sure_that(strcmp(my_argv[2], "Foo") == 0);
        make_sure_that(strcmp(my_argv[3], "-y") == 0);
        make_sure_that(strcmp(my_argv[4], "Hieperdepiep") == 0);

        clear_args(my_argc, my_argv);
    }

    {
        char *my_argv[] = {
            "test6", "-N"
        };

        int my_argc = sizeof(my_argv) / sizeof(my_argv[0]);

        char *mx_name = NULL;

        int r = mxOption('N', "mx-name", &my_argc, my_argv, &mx_name);

        make_sure_that(r == -1);
        make_sure_that(my_argc == 2);
        make_sure_that(mx_name == NULL);

        make_sure_that(strcmp(mxError(), "Missing argument for option -N.\n") == 0);

        clear_args(my_argc, my_argv);
    }

    {
        char *my_argv[] = {
            "test6", "-N", "-x"
        };

        int my_argc = sizeof(my_argv) / sizeof(my_argv[0]);

        char *mx_name = NULL;

        int r = mxOption('N', "mx-name", &my_argc, my_argv, &mx_name);

        make_sure_that(r == -1);
        make_sure_that(my_argc == 3);
        make_sure_that(mx_name == NULL);

        make_sure_that(strcmp(mxError(), "Missing argument for option -N.\n") == 0);

        clear_args(my_argc, my_argv);
    }

    {
        char *my_argv[] = {
            "test6", "--mx-name"
        };

        int my_argc = sizeof(my_argv) / sizeof(my_argv[0]);

        char *mx_name = NULL;

        int r = mxOption('N', "mx-name", &my_argc, my_argv, &mx_name);

        make_sure_that(r == -1);
        make_sure_that(my_argc == 2);
        make_sure_that(mx_name == NULL);

        make_sure_that(strcmp(mxError(), "Missing argument for option --mx-name.\n") == 0);

        clear_args(my_argc, my_argv);
    }

    {
        char *my_argv[] = {
            "test6", "--mx-name", "-x"
        };

        int my_argc = sizeof(my_argv) / sizeof(my_argv[0]);

        char *mx_name = NULL;

        int r = mxOption('N', "mx-name", &my_argc, my_argv, &mx_name);

        make_sure_that(r == -1);
        make_sure_that(my_argc == 3);
        make_sure_that(mx_name == NULL);

        make_sure_that(strcmp(mxError(), "Missing argument for option --mx-name.\n") == 0);

        clear_args(my_argc, my_argv);
    }

    return errors;
}
