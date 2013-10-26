/*
 * private.c: Private functions not meant for the user.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: private.c 101 2013-10-10 18:36:03Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdlib.h>
#include <unistd.h>

#include <libjvs/defs.h>
#include <libjvs/debug.h>

#include "private.h"

#define MIN_PORT 1024
#define MAX_PORT 65535

/*
 * Return the mx_name to use if <mx_name> was given to mxConnect(). If it is a valid name (i.e. not
 * NULL) use it. Otherwise use the environment variable MX_NAME if it is set. Otherwise use the
 * environment variable USER (the user's login name) if it is set. Otherwise give up and return
 * NULL.
 */
const char *mx_get_name(const char *mx_name)
{
    if (mx_name != NULL)
        return mx_name;
    else if ((mx_name = getenv("MX_NAME")) != NULL)
        return mx_name;
    else if ((mx_name = getenv("USER")) != NULL)
        return mx_name;
    else
        return NULL;
}

/*
 * Return the listen port that the master component will use for the message directory with name
 * <mx_name>.
 */
uint16_t mx_get_port(const char *mx_name)
{
    const char *p;
    int hash = 0;

    for (p = mx_name; *p != '\0'; p++) {
        hash += *p * 307;
    }

    return MIN_PORT + (hash % (MAX_PORT - MIN_PORT + 1));
}

/*
 * Return the mx_host to use if <mx_host> was given to mxConnect(). If it is a valid name (i.e. not
 * NULL) use it. Otherwise use the environment variable MX_HOST if it is set. Otherwise simply use
 * "localhost".
 */
const char *mx_get_host(const char *mx_host)
{
    if (mx_host != NULL)
        return mx_host;
    else if ((mx_host = getenv("MX_HOST")) != NULL)
        return mx_host;
    else
        return "localhost";
}
