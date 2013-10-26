#ifndef PRIVATE_H
#define PRIVATE_H

/*
 * private.h: Private functions not meant for the user.
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: private.h 121 2013-10-15 20:00:35Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <libjvs/ml.h>
#include <libjvs/ns.h>
#include <libjvs/ns-types.h>
#include <libjvs/pa.h>
#include <libjvs/hash.h>
#include <libjvs/list.h>

#include <stdint.h>

#include "libmx.h"

typedef uint16_t MX_Id;                 /* MX component ID. */

enum {
   MX_QUIT_REQUEST,
   MX_HELLO_REQUEST,
   MX_HELLO_REPLY,
   MX_HELLO_REPORT,
   MX_HELLO_UPDATE,
   MX_REGISTER_REQUEST,
   MX_REGISTER_REPLY,
   MX_REGISTER_REPORT,
   MX_SUBSCRIBE_UPDATE,
   MX_CANCEL_UPDATE,
   MX_PUBLISH_UPDATE,
   MX_WITHDRAW_UPDATE,
   NUM_MX_MESSAGES
};

/*
 * Return the mx_name to use if <mx_name> was given to mxConnect(). If it is a valid name (i.e. not
 * NULL) use it. Otherwise use the environment variable MX_NAME if it is set. Otherwise use the
 * environment variable USER (the user's login name) if it is set. Otherwise give up and return
 * NULL.
 */
const char *mx_get_name(const char *mx_name);

/*
 * Return the listen port that the master component will use for the message directory with name
 * <mx_name>.
 */
uint16_t mx_get_port(const char *mx_name);

/*
 * Return the mx_host to use if <mx_host> was given to mxConnect(). If it is a valid name (i.e. not
 * NULL) use it. Otherwise use the environment variable MX_HOST if it is set. Otherwise simply use
 * "localhost".
 */
const char *mx_get_host(const char *mx_host);

#endif
