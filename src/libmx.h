#ifndef LIBMX_H
#define LIBMX_H

/*
 * libmx.h: Description
 *
 * Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:	$Id: libmx.h 121 2013-10-15 20:00:35Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

typedef struct MX MX;           /* MX struct. */

typedef uint32_t MX_Type;       /* MX message type. */
typedef uint32_t MX_Size;       /* MX message payload size. */
typedef uint32_t MX_Version;    /* MX message version number. */

/*
 * Connect to the master component for message exchange <mx_name> on host <mx_host>. Introduce
 * myself as <my_name>. Returns a pointer to an MX struct. If <mx_name> is NULL, the environment
 * variable MX_NAME is used, or, if it doesn't exist, the current user name. If <mx_host> is NULL
 * it is set to the environment variable MX_HOST, or, if it doesn't exist, to "localhost". <my_name>
 * can not be NULL.
 */
MX *mxConnect(const char *mx_name, const char *mx_host, const char *my_name);

/*
 * Register a message type with name <msg_name> using <mx>, and return the associated message type
 * id.
 */
MX_Type mxRegister(MX *mx, const char *msg_name);

/*
 * Subscribe to messages of type <type> through <mx>. When a message of this type comes in call <cb>
 * with the properties of this message and the <udata> that was passed in here. The payload that is
 * passed to <cb> is the callee's responsibility, and they should free it when it is no longer
 * needed.
 */
void mxSubscribe(MX *mx, MX_Type type, void (*cb)(MX *mx, int fd, MX_Type type, MX_Version version,
                   MX_Size size, char *payload, void *udata), void *udata);

/*
 * Cancel your subscription to messages of type <type>.
 */
void mxCancel(MX *mx, MX_Type type);

/*
 * Announce that you'll be broadcasting messages of type <type>. This function must be called before
 * using mxBroadcast (or mxVaBroadcast). It is not necessary (but allowed) if you only use mxWrite,
 * mxPack or mxVaPack for this message type.
 */
void mxPublish(MX *mx, MX_Type type);

/*
 * Withdraw the publication of messages of type <type>. No more messages of this type may be
 * broadcast after a call to this function.
 */
void mxWithdraw(MX *mx, MX_Type type);

/*
 * Add a timer at time <t> to <mx>, and call <cb> with <udata> when it goes off.
 */
void mxOnTime(MX *mx, double t, void (*cb)(MX *mx, double t, void *udata), const void *udata);

/*
 * Drop the previously added timer at time <t> with callback <cb> in <mx>. Both <t> and <cb> must
 * match the previously set timer.
 */
void mxDropTime(MX *mx, double t, void (*cb)(MX *mx, double t, void *udata));

/*
 * Tell <mx> to call <cb> with <udata> when file descriptor <fd> has data avialable. <fd> in this
 * case represents a file descriptor opened by the user themselves, not one managed by <mx>.
 */
void mxOnData(MX *mx, int fd, void (*cb)(MX *mx, int fd, void *udata), const void *udata);

/*
 * Tell <mx> to stop listening for data on <fd>. Again, <fd> is a file descriptor opened by the
 * user, not one managed by <mx>.
 */
void mxDropData(MX *mx, int fd);

/*
 * Call <cb> when a new component reports in. The file descriptor that the component is connected to
 * and its name are passed to <cb>, along with <udata>.
 */
void mxOnNewComponent(MX *mx, void (*cb)(MX *mx, int fd, const char *name, void *udata), const void
                        *udata);

/*
 * Call <cb> when a component disconnects. The file descriptor where it was connected and its name
 * are passed to <cb> along with <udata>.
 */
void mxOnEndComponent(MX *mx, void (*cb)(MX *mx, int fd, const char *name, void *udata), const void
                        *udata);

/*
 * Arrange for <cb> to be called if a new subscriber on message type <type> announces itself. Not
 * called for own subscriptions created with mxSubscribe.
 */
void mxOnNewSubscriber(MX *mx, MX_Type type, void (*cb)(MX *mx, MX_Type type, int fd, void *udata),
                         const void *udata);

/*
 * Arrange for <cb> to be called if a new publisher of message type <type> announces itself. Not
 * called for own subscriptions created with mxPublish.
 */
void mxOnNewPublisher(MX *mx, MX_Type type, void (*cb)(MX *mx, MX_Type type, int fd, void *udata),
                        const void *udata);

/*
 * Arrange for <cb> to be called if a subscriber to messages of type <type> exits or cancels its
 * subscription. Not called on mxCancel.
 */
void mxOnEndSubscriber(MX *mx, MX_Type type, void (*cb)(MX *mx, MX_Type type, int fd, void *udata),
                         const void *udata);

/*
 * Arrange for <cb> to be called if a publisher of messages of type <type> exits or withdraws its
 * publication. Not called on mxWithdraw.
 */
void mxOnEndPublisher(MX *mx, MX_Type type, void (*cb)(MX *mx, MX_Type type, int fd, void *udata),
                        const void *udata);

/*
 * Call <cb> when a new message is registered.
 */
void mxOnNewMessage(MX *mx,
        void (*cb)(MX *mx, const char *name, MX_Type type, void *udata), const void *udata);

/*
 * Return the number of publishers of messages of type <type>.
 */
int mxPublisherCount(MX *mx, MX_Type type);

/*
 * Return the number of subscribers to messages of type <type>.
 */
int mxSubscriberCount(MX *mx, MX_Type type);

/*
 * Return the name of the component at file descriptor <fd> on <mx>.
 */
const char *mxComponentName(MX *mx, int fd);

/*
 * Return the name of message type <type>.
 */
const char *mxMessageName(MX *mx, MX_Type type);

/*
 * Write <payload>, which has size <size>, in a message of type <type> with version <version to file
 * descriptor <fd> over message exchange <mx>.
 */
void mxWrite(MX *mx, int fd, MX_Type type, MX_Version version, const char *payload, size_t size);

/*
 * Write a message of type <type> with version <version> to file descriptor <fd> over message
 * exchange <mx>. The payload of the message is constructed using the PACK_* method as described in
 * libjvs/utils.h.
 */
void mxPack(MX *mx, int fd, MX_Type type, MX_Version version, ...);

/*
 * Write a message of type <type> with version <version> to file descriptor <fd> over message
 * exchange <mx>. The payload of the message is constructed using the PACK_* arguments contained in
 * <ap>, as described in libjvs/utils.h.
 */
void mxVaPack(MX *mx, int fd, MX_Type type, MX_Version version, va_list ap);

/*
 * Broadcast a message with type <type> and version <version> to all subscribers of this message
 * type. The contents of the message are set using the "astrpack" interface from libjvs/utils.h.
 * Publication of this message type must have been announced with the mxPublish function.
 */
void mxBroadcast(MX *mx, MX_Type type, MX_Version version, ...);

/*
 * Broadcast a message with type <type> and version <version> to all subscribers of this message
 * type. The contents of the message are set using the "vastrpack" interface from libjvs/utils.h.
 * Publication of this message type must have been announced with the mxPublish function.
 */
void mxVaBroadcast(MX *mx, MX_Type type, MX_Version version, va_list ap);

/*
 * Wait for a message of type <type> to arrive over file descriptor <fd> on message exchange <mx>.
 * The version of the received message is returned through <version> and its payload through
 * <payload>. If the message arrives within <timeout> seconds, the function returns 0. If it doesn't
 * it returns 1. If some other (network) error occurs, it returns -1.
 */
int mxAwait(MX *mx, int fd, MX_Type type, MX_Version *version, MX_Size *size, char **payload,
              double timeout);

/*
 * Run the message exchange <mx>. This function will return 0 if there are no more timers or
 * connections to wait for, or -1 if an error occurred.
 */
int mxRun(MX *mx);

/*
 * Close down message exchange <mx>. This will cause a call to mxRun or mxAwait to terminate.
 */
void mxClose(MX *mx);

/*
 * Destroy message exchange <mx>. Do not call this inside mxRun. Instead, call mxClose and wait for
 * mxRun to terminate, then call mxDestroy.
 */
void mxDestroy(MX *mx);

#endif
