#ifndef LIBMX_H
#define LIBMX_H

/*
 * libmx.h: Main interface to libmx.
 *
 * Copyright:	(c) 2014 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: libmx.h 448 2020-01-06 09:02:39Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdint.h>

typedef struct MX MX;

/*
 * Return the mx_name to use if <mx_name> was given to mxClient() or mxMaster().
 * If it is a valid name (i.e. not NULL) use it. Otherwise use the environment
 * variable MX_NAME if it is set. Otherwise use the environment variable USER
 * (the user's login name) if it is set. Otherwise give up and return NULL.
 */
const char *mxEffectiveName(const char *mx_name);

/*
 * Return the listen port that the master component will use for mx name
 * <mx_name>.
 */
uint16_t mxEffectivePort(const char *mx_name);

/*
 * Return the mx_host to use if <mx_host> was given to mxClient() or mxMaster().
 * If it is a valid name (i.e. not NULL) use it. Otherwise use the environment
 * variable MX_HOST if it is set. Otherwise simply use "localhost".
 */
const char *mxEffectiveHost(const char *mx_host);

/*
 * Scan the options given by <argc> and <argv> and extract the argument given
 * with the <short_name> or <long_name> option. If found, the argument is
 * returned through <argument> and the option and associated argument are
 * removed from the option list. If not, <argument> is set to NULL and <argc>
 * and <argv> are unchanged.
 *
 * If the given option is present but it has no argument, -1 is returned and an
 * error message can be retrieved through mxError(). Otherwise 0 is returned.
 *
 * <short_name> and <long_name> should be given without the leading "-" or "--".
 *
 * The following formats are allowed:
 *
 *      -N Bla
 *      -NBla
 *      --mx-name Bla
 *      --mx-name=Bla
 */
int mxOption(char short_name, const char *long_name, int *argc, char *argv[], char **argument);

/*
 * Create and return an MX struct that will act as a client, connecting to the
 * Message Exchange with name <mx_name> running on host <mx_host>. We will
 * introduce ourselves as <my_name>.
 *
 * If <mx_host> is NULL, the environment variable MX_HOST is used. If that
 * doesn't exist, "localhost" is used.
 * If <mx_name> is NULL, the environment variable MX_NAME is used. If that
 * doesn't exist, the environment variable USER is used. If that doesn't exist
 * either, the function fails and NULL is returned.
 * If <my_name> is NULL, the function fails and returns NULL.
 *
 * When this function finishes successfully, a listen port has been opened
 * for other components to connect to. No other connections have been made, and
 * no communication threads have been started yet (use mxBegin() for this).
 *
 * This function exists for applications that need to do "stuff" after the
 * listen port is opened but before the communication threads are started. Most
 * applications will want to use the mxClient() function further down, which
 * simply calls mxCreateClient() followed by mxBegin().
 */
MX *mxCreateClient(const char *mx_host, const char *mx_name, const char *my_name);

/*
 * Create and return an MX struct that will act as a master for the Message
 * Exchange with name <mx_name>, running on the local host and using the name
 * <my_name>.
 *
 * If <mx_name> is NULL, the environment variable MX_NAME is used. If that
 * doesn't exist, the environment variable USER is used. If that doesn't exist
 * either, the function fails and NULL is returned.
 * If <my_name> is NULL, "master" is used.
 *
 * When this function finishes successfully, a listen port has been opened
 * for other components to connect to. No other connections have been made, and
 * no communication threads have been started yet (use mxBegin() for this).
 *
 * This function exists for applications that need to do "stuff" after the
 * listen port is opened but before the communication threads are started (such
 * as the "mx" command). Most applications will want to use the mxMaster()
 * function further down, which simply calls mxCreateMaster() followed by
 * mxBegin().
 */
MX *mxCreateMaster(const char *mx_name, const char *my_name);

/*
 * Begin running the threads that listen for connection and timer events.
 */
int mxBegin(MX *mx);

/*
 * Create and return an MX struct that will act as a master for the Message
 * Exchange with name <mx_name>, running on the local host.
 *
 * If <mx_name> is NULL, the environment variable MX_NAME is used. If that
 * doesn't exist, the environment variable USER is used. If that doesn't exist
 * either, the function fails and NULL is returned.
 * If <my_name> is NULL, "master" is used.
 *
 * When this function returns, a listen port has been opened for clients to
 * connect to, and the necessary background threads will also be started.
 */
MX *mxMaster(const char *mx_name, const char *my_name);

/*
 * Create and return an MX struct that will act as a client for the Message
 * Exchange with name <mx_name>, running on <mx_host>.
 *
 * If <mx_host> is NULL, the environment variable MX_HOST is used. If that
 * doesn't exist, "localhost" is used.
 * If <mx_name> is NULL, the environment variable MX_NAME is used. If that
 * doesn't exist, the environment variable USER is used. If that doesn't exist
 * either, the function fails and NULL is returned.
 * If <my_name> is NULL, the function fails and returns NULL.
 *
 * When this function finishes successfully, a listen port has been opened for
 * other components to connect to, and the necessary background threads will
 * also be started.
 */
MX *mxClient(const char *mx_host, const char *mx_name, const char *my_name);

/*
 * Return the file descriptor on which all events associated with <mx> arrive.
 */
int mxConnectionNumber(MX *mx);

/*
 * Process any pending events associated with <mx>. Returns -1 if an error
 * occurred, 1 if event processing has finished normally and 0 if no more events
 * are forthcoming (so there's no sense in waiting for them anymore).
 */
int mxProcessEvents(MX *mx);

/*
 * Loop while listening for and handling events. Returns -1 if an error occurred
 * or 0 if mxShutdown was called.
 */
int mxRun(MX *mx);

/*
 * Return the name of the local component.
 */
const char *mxMyName(const MX *mx);

/*
 * Return the current MX name.
 */
const char *mxName(const MX *mx);

/*
 * Return the current MX host, i.e. the host where the master component is
 * running.
 */
const char *mxHost(const MX *mx);

/*
 * Return the current MX port, i.e. the port on which the master listens.
 */
uint16_t mxPort(const MX *mx);

/*
 * Register the message named <msg_name>. Returns the associated message type
 * id.
 */
uint32_t mxRegister(MX *mx, const char *msg_name);

/*
 * Returns the name of message type <type>.
 */
const char *mxMessageName(MX *mx, uint32_t type);

/*
 * Returns the name of the component connected on fd <fd>.
 */
const char *mxComponentName(MX *mx, int fd);

/*
 * Subscribe to messages of type <type>. <handler> will be called for all
 * incoming messages of this type, passing in the same <udata> that is passed in
 * to this function. Returns <0 on errors, >0 on notices and 0 otherwise. Check
 * mxError() when return value is not 0.
 */
int mxSubscribe(MX *mx, uint32_t type,
        void (*handler)(MX *mx, int fd, uint32_t type, uint32_t version,
            char *payload, uint32_t size, void *udata),
        void *udata);

/*
 * Cancel our subscription to messages of type <type> that calls <handler>.
 * Returns <0 on errors, >0 on notices and 0 otherwise. Check mxError() when
 * return value is not 0.
 */
int mxCancel(MX *mx, uint32_t type);

/*
 * Call <handler> for new subscribers to message type <type>, passing in the
 * same <udata> that was passed in here.
 */
void mxOnNewSubscriber(MX *mx, uint32_t type,
        void (*handler)(MX *mx, int fd, uint32_t type, void *udata),
        void *udata);

/*
 * Call <handler> when a subscriber cancels their subscription to messages of
 * type <type>.
 */
void mxOnEndSubscriber(MX *mx, uint32_t type,
        void (*handler)(MX *mx, int fd, uint32_t type, void *udata),
        void *udata);

/*
 * Call <handler> when a new component reports in. <handler> is called with the
 * file descriptor through which we're connected to the new component in <fd>,
 * and its name in <name>.
 */
void mxOnNewComponent(MX *mx,
        void (*handler)(MX *mx, int fd, const char *name, void *udata),
        void *udata);

/*
 * Call <handler> when connection with a component is lost. <handler> is called
 * with the file descriptor through which we were connected to the new component
 * in <fd>, and its name in <name>.
 */
void mxOnEndComponent(MX *mx,
        void (*handler)(MX *mx, int fd, const char *name, void *udata),
        void *udata);

/*
 * Call <handler> when a new message type is registered.
 */
void mxOnNewMessage(MX *mx,
        void (*handler)(MX *mx, uint32_t type, const char *name, void *udata),
        void *udata);

/*
 * Send a message of type <type> to file descriptor <fd>.
 */
void mxSend(MX *mx, int fd, uint32_t type, uint32_t version, const void *payload, uint32_t size);

/*
 * Write a message of type <type> with version <version> to file descriptor <fd>
 * over message exchange <mx>. The payload of the message is constructed using
 * the PACK_* method as described in libjvs/utils.h.
 */
void mxPackAndSend(MX *mx, int fd, uint32_t type, uint32_t version, ...);

/*
 * Write a message of type <type> with version <version> to file descriptor <fd>
 * over message exchange <mx>. The payload of the message is constructed using
 * the PACK_* arguments contained in <ap>, as described in libjvs/utils.h.
 */
void mxVaPackAndSend(MX *mx, int fd, uint32_t type, uint32_t version, va_list ap);

/*
 * Broadcast a message with type <type>, version <version> and payload <payload>
 * with size <size> to all subscribers of this message type.
 */
void mxBroadcast(MX *mx, uint32_t type, uint32_t version, const void *payload, uint32_t size);

/*
 * Broadcast a message with type <type> and version <version> to all subscribers
 * of this message type. The contents of the message are set using the
 * "astrpack" interface from libjvs/utils.h.
 */
void mxPackAndBroadcast(MX *mx, uint32_t type, uint32_t version, ...);

/*
 * Broadcast a message with type <type> and version <version> to all subscribers
 * of this message type. The contents of the message are set using the
 * "vastrpack" interface from libjvs/utils.h.
 */
void mxVaPackAndBroadcast(MX *mx, uint32_t type, uint32_t version, va_list ap);

/*
 * Wait for a message of type <type> to arrive on file descriptor <fd>. If the
 * message arrives within <timeout> seconds, 1 is returned and the version,
 * payload and payload size of the message are returned via <version>, <payload>
 * and <size>. Otherwise, 0 is returned and <version>, <payload> and <size> are
 * unchanged.
 *
 * If successful, <payload> points to a newly allocated memory buffer. It is the
 * caller's responsibility to free it when it is no longer needed.
 */
int mxAwait(MX *mx, int fd, double timeout,
        uint32_t type, uint32_t *version, char **payload, uint32_t *size);

/*
 * Send a message of type <request_type> with version <request_version> and the
 * payload that follows (specified using the "astrpack" interface from
 * libjvs/utils.h) to file descriptor <fd>, and wait for a reply with type
 * <reply_type>. If the reply arrives within <timeout> seconds, 1 is returned
 * and the version, payload and payload size of the reply are returned via
 * <reply_version>, <reply_payload> and <reply_size>. Otherwise, 0 is returned
 * and <reply_version>, <reply_payload> and <reply_size> are unchanged.
 *
 * If successful, <reply_payload> points to a newly allocated memory buffer. It
 * is the caller's responsibility to free it when it is no longer needed.
 */
int mxPackAndWait(MX *mx, int fd, double timeout,
        uint32_t reply_type, uint32_t *reply_version,
        char **reply_payload, uint32_t *reply_size,
        uint32_t request_type, uint32_t request_version, ...);

/*
 * Send a message of type <request_type> with version <request_version> and the
 * payload that follows in <ap> (specified using the "astrpack" interface from
 * libjvs/utils.h) to file descriptor <fd>, and wait for a reply with type
 * <reply_type>. If the reply arrives within <timeout> seconds, 1 is returned
 * and the version, payload and payload size of the reply are returned via
 * <reply_version>, <reply_payload> and <reply_size>. Otherwise, 0 is returned
 * and <reply_version>, <reply_payload> and <reply_size> are unchanged.
 *
 * If successful, <reply_payload> points to a newly allocated memory buffer. It
 * is the caller's responsibility to free it when it is no longer needed.
 */
int mxVaPackAndWait(MX *mx, int fd, double timeout,
        uint32_t reply_type, uint32_t *reply_version,
        char **reply_payload, uint32_t *reply_size,
        uint32_t request_type, uint32_t request_version, va_list ap);

/*
 * Send a message of type <request_type> with version <request_version>, payload
 * <request_payload> and payload size <request_size> to file descriptor <fd>,
 * and wait for a reply with type <reply_type>. If the reply arrives within
 * <timeout> seconds, 1 is returned and the version, payload and payload size of
 * the reply are returned via <reply_version>, <reply_payload> and <reply_size>.
 * Otherwise, 0 is returned and <reply_version>, <reply_payload> and
 * <reply_size> are unchanged.
 *
 * If successful, <reply_payload> points to a newly allocated memory buffer. It
 * is the caller's responsibility to free it when it is no longer needed.
 */
int mxSendAndWait(MX *mx, int fd, double timeout,
        uint32_t reply_type, uint32_t *reply_version,
        char **reply_payload, uint32_t *reply_size,
        uint32_t request_type, uint32_t request_version,
        const char *request_payload, uint32_t request_size);

/*
 * Create a timer that will call <handler> at time <t> (seconds since the UNIX
 * epoch). In future calls to mxAdjustTimer and mxRemoveTimer this timer will be
 * identified by <id>. When calling <handler>, the same pointer <udata> given
 * here will be passed back.
 */
void mxCreateTimer(MX *mx, uint32_t id, double t,
        void (*handler)(MX *mx, uint32_t id, double t, void *udata),
        void *udata);

/*
 * Adjust the time of the timer with id <id> to <t>.
 */
void mxAdjustTimer(MX *mx, uint32_t id, double t);

/*
 * Remove the timer with id <id>. This timer will not be triggered after all.
 */
void mxRemoveTimer(MX *mx, uint32_t id);

/*
 * Return the current UTC timestamp as a double.
 */
double mxNow(void);

/*
 * Shut down <mx>. After this function is called, the mxRun function will
 * return.
 */
void mxShutdown(MX *mx);

/*
 * Destroy <mx>. Call this function only after mxRun has returned.
 */
void mxDestroy(MX *mx);

/*
 * Return a text representation of the errors that have occurred up to now. The
 * returned string becomes the property of the caller, who is responsible for
 * freeing it. Calling this function empties the error string.
 */
char *mxError(void);

#ifdef __cplusplus
}
#endif

#endif
