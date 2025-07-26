/*
 * libmx.c: Main interface to libmx.
 *
 * Copyright: (c) 2014-2025 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Version:   $Id: libmx.c 460 2022-01-29 19:32:32Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <sys/socket.h>

#include <libjvs/pa.h>
#include <libjvs/net.h>
#include <libjvs/tcp.h>
#include <libjvs/buffer.h>
#include <libjvs/utils.h>
#include <libjvs/hash.h>
#include <libjvs/debug.h>

#include "types.h"
#include "libmx.h"
#include "msg.h"
#include "cmd.h"
#include "evt.h"

#define MIN_PORT 1024
#define MAX_PORT 65535

static Buffer mx_message = { 0 };

/* Severity of the last error. */

static enum {
    MX_NOTICE,
    MX_ERROR
} mx_severity = MX_NOTICE;

/*
 * Set the current error message to <fmt> with the subsequent parameters, and
 * the severity to <severity>.
 */
static void mx_set_message(int severity, char *fmt, va_list ap)
{
    bufAddV(&mx_message, fmt, ap);

    mx_severity = severity;
}

/*
 * Set the current error message to <fmt> with the subsequent parameters, and
 * the severity to MX_ERROR.
 */
__attribute__ ((format (printf, 1, 2)))
static void mx_error(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    mx_set_message(MX_ERROR, fmt, ap);
    va_end(ap);
}

/*
 * Set the current error message to <fmt> with the subsequent parameters, and
 * the severity to MX_NOTICE.
 */
__attribute__ ((format (printf, 1, 2)))
static void mx_notice(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    mx_set_message(MX_NOTICE, fmt, ap);
    va_end(ap);
}

/*
 * Convert the double-precision timestamp in <t> to the timespec in <ts>.
 */
static void double_to_timespec(double t, struct timespec *ts)
{
    if (t <= 0) {
        ts->tv_sec  = 0;
        ts->tv_nsec = 0;
    }
    else {
        ts->tv_sec  = t;
        ts->tv_nsec = (t - ts->tv_sec) * 1000000000L;
    }
}

/*
 * Initialize command queue <queue>.
 */
static void mx_init_queue(MX_Queue *queue)
{
    pthread_mutex_init(&queue->ok_to_access, NULL);

    sem_init(&queue->ok_to_read, 0, 0);
}

/*
 * Push command <cmd> onto queue <queue>.
 */
static void mx_push_command(MX_Queue *queue, MX_Command *cmd)
{
    pthread_mutex_lock(&queue->ok_to_access);

    listAppendTail(&queue->commands, cmd);

    pthread_mutex_unlock(&queue->ok_to_access);

    sem_post(&queue->ok_to_read);
}

/*
 * Pop a command off of <queue> and return it. This function will block until a
 * command is available. <deadline> may point to a double precision timestamp,
 * in which case this function will wait until that time for a command. If
 * <deadline> is NULL this function will wait indefinitely.
 * If <deadline> is given and no command arrives on time this function returns
 * NULL. Otherwise it returns the received command.
 */
static MX_Command *mx_pop_command(MX_Queue *queue, double *deadline)
{
    int r;

    if (deadline) {
        struct timespec time_spec;

        double_to_timespec(*deadline, &time_spec);

        r = sem_timedwait(&queue->ok_to_read, &time_spec);
    }
    else {
        r = sem_wait(&queue->ok_to_read);
    }

    if (r == 0) {
        pthread_mutex_lock(&queue->ok_to_access);

        MX_Command *cmd = listRemoveHead(&queue->commands);

        pthread_mutex_unlock(&queue->ok_to_access);

        return cmd;
    }
    else {
        return NULL;
    }
}

/*
 * Create and return a new event of type <type>.
 */
static MX_Event *mx_new_event(MX_EventType type)
{
    MX_Event *evt = calloc(1, sizeof(*evt));

    evt->evt_type = type;

    return evt;
}

/*
 * Create and return a new MX_ET_CONN event. The new component's file
 * descriptor is <fd>.
 */
static MX_Event *mx_connect_event(int fd)
{
    MX_Event *evt = mx_new_event(MX_ET_CONN);

    evt->u.conn.fd = fd;

    return evt;
}

/*
 * Create and return a new MX_ET_MSG event. The received message had type
 * <type>, version <version> and payload <payload> with size <size>.
 */
static MX_Event *mx_message_event(int fd,
        uint32_t type, uint32_t version, char *payload, uint32_t size)

{
    MX_Event *evt = mx_new_event(MX_ET_MSG);

    evt->u.msg.fd  = fd;
    evt->u.msg.msg_type = type;
    evt->u.msg.version = version;
    evt->u.msg.payload = payload;
    evt->u.msg.size = size;

    return evt;
}

/*
 * Create and return a new MX_ET_ERR event. The error occurred on file
 * descriptor <fd> and had errno code <error>. <whence> is the function that
 * returned the error.
 */
static MX_Event *mx_error_event(int fd, const char *whence, int error)
{
    MX_Event *evt = mx_new_event(MX_ET_ERR);

    evt->u.err.fd     = fd;
    evt->u.err.whence = strdup(whence);
    evt->u.err.error  = error;

    return evt;
}

/*
 * Create and return a new MX_ET_DISC event. The disconnected file descriptor
 * was <fd>; the function where the disconnect was first noticed was <whence>.
 */
static MX_Event *mx_disc_event(int fd, const char *whence)
{
    MX_Event *evt = mx_new_event(MX_ET_DISC);

    evt->u.disc.fd = fd;
    evt->u.disc.whence = strdup(whence);

    return evt;
}

/*
 * Create and return a new MX_ET_TIMER event. The time at which the timer went
 * off is <t>, the handler is <handler> and the udata to be passed in to it is
 * <udata>.
 */
static MX_Event *mx_timer_event(double t, uint32_t id,
        void (*handler)(MX *mx, uint32_t id, double t, void *udata), void *udata)
{
    MX_Event *evt = mx_new_event(MX_ET_TIMER);

    evt->u.timer.t = t;
    evt->u.timer.id = id;
    evt->u.timer.handler = handler;
    evt->u.timer.udata = udata;

    return evt;
}

/*
 * Create a write command with <msg_type>, <version>, <payload> and payload size <size>.
 */
static MX_Command *mx_create_write_command(uint32_t msg_type, uint32_t version,
        const char *payload, uint32_t size)
{
    MX_Command *cmd = calloc(1, sizeof(*cmd));

    cmd->cmd_type = MX_CT_WRITE;
    cmd->u.write.msg_type = msg_type;
    cmd->u.write.version = version;
    cmd->u.write.size = size;

    cmd->u.write.payload = memdup(payload, size);

    return cmd;
}

/*
 * Create a command that instructs the writer or timer threads to exit.
 */
static MX_Command *mx_create_exit_command(void)
{
    MX_Command *cmd = calloc(1, sizeof(*cmd));

    cmd->cmd_type = MX_CT_EXIT;

    return cmd;
}

/*
 * Send pointer <ptr> over file descriptor <fd>.
 */
static int mx_send_pointer(int fd, void *ptr)
{
    int r = write(fd, &ptr, sizeof(ptr));

    return r == sizeof(ptr) ? 0 : -1;
}

/*
 * Send a message of type <type>, with version <version>, payload <payload> and
 * payload size <size> to component <comp>.
 */
static void mx_send(MX_Component *comp,
        uint32_t type, uint32_t version,
        const char *payload, uint32_t size)
{
    MX_Command *cmd;

    cmd = mx_create_write_command(type, version, payload, size);

    mx_push_command(&comp->writer_queue, cmd);
}

/*
 * Pack a message with type <type> and version <version> with the fields in <ap>
 * to component <comp>.
 */
static void mx_va_pack(MX_Component *comp,
        uint32_t type, uint32_t version, va_list ap)
{
    char *payload;
    size_t size = vastrpack(&payload, ap);

    dbgAssert(stderr, size <= MAX_PAYLOAD_SIZE, "payload too large.\n");

    mx_send(comp, type, version, payload, size);

    free(payload);
}

/*
 * Pack a message with type <type> and version <version> and the subsequent
 * fields to component <comp>.
 */
static void mx_pack(MX_Component *comp,
        uint32_t type, uint32_t version, ...)
{
    va_list ap;

    va_start(ap, version);
    mx_va_pack(comp, type, version, ap);
    va_end(ap);
}

/*
 * Add an await struct to the component associated with component <comp>.
 */
static MX_Await *mx_add_await(MX_Component *comp, uint32_t type)
{
    MX_Await *await = calloc(1, sizeof(*await));

    await->msg_type = type;

    pthread_mutex_init(&await->mutex, NULL);
    pthread_mutex_lock(&await->mutex);

    pthread_rwlock_wrlock(&comp->await_lock);

    listAppendTail(&comp->awaits, await);

    pthread_rwlock_unlock(&comp->await_lock);

    return await;
}

/*
 * Send a message of type <request_type> with version <request_version>, payload
 * <request_payload> and payload size <request_size> to component <comp>, and
 * wait for a reply with type <reply_type>. If the reply arrives within
 * <timeout> seconds, 0 is returned and the version, payload and payload size of
 * the reply are returned via <reply_version>, <reply_payload> and <reply_size>.
 * If the timeout expires before that, 1 is returned and if an error occurs -1
 * is returned. In both these cases <reply_version>, <reply_payload> and
 * <reply_size> are unchanged.
 *
 * If successful, <reply_payload> points to a newly allocated memory buffer. It
 * is the caller's responsibility to free it when it is no longer needed.
 */
static int mx_send_and_wait(MX_Component *comp, double timeout,
        uint32_t reply_type, uint32_t *reply_version,
        char **reply_payload, uint32_t *reply_size,
        uint32_t request_type, uint32_t request_version,
        const char *request_payload, uint32_t request_size)
{
    int r;
    struct timespec deadline;
    MX_Await *await;

    /* Determine absolute timeout value. */

    double_to_timespec(mxNow() + timeout, &deadline);

    /* Add await info to the component. */

    await = mx_add_await(comp, reply_type);

    /* Send the request that we expect an answer to. */

    mx_send(comp, request_type, request_version, request_payload, request_size);

    /* Now wait until the reader thread unlocks the created mutex. */

    r = pthread_mutex_timedlock(&await->mutex, &deadline);

    if (r == 0) {
        /* Reader thread unlocked the mutex (and removed the await struct from
         * the await list): reply did arrive. Return reply info to the caller.
         */
        *reply_version = await->version;
        *reply_payload = await->payload;
        *reply_size = await->size;
    }
    else {
        /* Timeout. We'll unlock the mutex ourselves (and remove the await
         * struct from the await list). */
        pthread_mutex_unlock(&await->mutex);

        listRemove(&comp->awaits, await);
    }

    /* Destroy the mutex. */
    pthread_mutex_destroy(&await->mutex);

    /* Destroy the await struct. */
    free(await);

    /* Set the appropriate return value. */
    if (r == 0) {
        return 0;
    }
    else if (r == ETIMEDOUT) {
        return 1;
    }
    else {
        return -1;
    }
}

/*
 * Pack a message of type <request_type> with version <request_version> and the
 * payload in <ap>, send it to component <comp>, and wait for a reply with type
 * <reply_type>. If the reply arrives within <timeout> seconds, 0 is returned
 * and the version, payload and payload size of the reply are returned via
 * <reply_version>, <reply_payload> and <reply_size>. If the timeout expires
 * before that, 1 is returned and if an error occurs -1 is returned. In both
 * these cases <reply_version>, <reply_payload> and <reply_size> are unchanged.
 *
 * If successful, <reply_payload> points to a newly allocated memory buffer. It
 * is the caller's responsibility to free it when it is no longer needed.
 */
static int mx_va_pack_and_wait(MX_Component *comp, double timeout,
        uint32_t reply_type, uint32_t *reply_version,
        char **reply_payload, uint32_t *reply_size,
        uint32_t request_type, uint32_t request_version, va_list ap)
{
    int r;
    char *payload;
    size_t size = vastrpack(&payload, ap);

    dbgAssert(stderr, size <= MAX_PAYLOAD_SIZE, "payload too large.\n");

    r = mx_send_and_wait(comp, timeout,
            reply_type, reply_version, reply_payload, reply_size,
            request_type, request_version, payload, size);

    free(payload);

    return r;
}

/*
 * Pack a message of type <request_type> with version <request_version> and the
 * subsequent payload to component <comp>, and wait for a reply with type
 * <reply_type>. If the reply arrives within <timeout> seconds, 0 is returned
 * and the version, payload and payload size of the reply are returned via
 * <reply_version>, <reply_payload> and <reply_size>. If the timeout expires
 * before that, 1 is returned and if an error occurs -1 is returned. In both
 * these cases <reply_version>, <reply_payload> and <reply_size> are unchanged.
 *
 * If successful, <reply_payload> points to a newly allocated memory buffer. It
 * is the caller's responsibility to free it when it is no longer needed.
 */
static int mx_pack_and_wait(MX_Component *comp, double timeout,
        uint32_t reply_type, uint32_t *reply_version,
        char **reply_payload, uint32_t *reply_size,
        uint32_t request_type, uint32_t request_version, ...)
{
    int r;
    va_list ap;

    va_start(ap, request_version);
    r = mx_va_pack_and_wait(comp, timeout,
            reply_type, reply_version, reply_payload, reply_size,
            request_type, request_version, ap);
    va_end(ap);

    return r;
}

/*
 * Create a new message type whose id is <type>.
 */
static MX_Message *mx_create_message(MX *mx, uint32_t type, const char *name)
{
    MX_Message *msg = calloc(1, sizeof(*msg));

    msg->msg_type = type;

    hashAdd(&mx->message_by_type, msg, HASH_VALUE(type));

    if (name != NULL) {
        msg->msg_name = strdup(name);

        hashAdd(&mx->message_by_name, msg, HASH_STRING(name));
    }

    if (mx->on_register_callback) {
        mx->on_register_callback(mx, type, name, mx->on_register_udata);
    }

    mx->next_message_type = MAX(mx->next_message_type, type + 1);

    return msg;
}

/*
 * Broadcast new message <msg> as an MX_MT_REGISTER_REPORT.
 */
static void mx_broadcast_new_message(MX *mx, MX_Message *msg, MX_Component *except)
{
    int fd;

    for (fd = 0; fd < paCount(&mx->components); fd++) {
        MX_Component *comp = paGet(&mx->components, fd);

        if (comp == NULL || comp == except) continue;

        mx_pack(comp, MX_MT_REGISTER_REPORT, 0,
                PACK_STRING,    msg->msg_name ? msg->msg_name : "",
                PACK_INT32,     msg->msg_type,
                END);
    }
}

/*
 * A thread that listens for new components. <arg> is a pointer to an MX
 * struct.
 */
static void *mx_listener_thread(void *arg)
{
    int new_fd;

    MX *mx = arg;

    /* Listen for components on listen_fd and report new connections via the
     * event_pipe. */

    while ((new_fd = tcpAccept(mx->listen_fd)) > 0) {
        mx_send_pointer(mx->event_pipe[WR], mx_connect_event(new_fd));
    }

    return NULL;
}

/*
 * Create the event pipe that subthreads can use to send events back to the main
 * loop.
 */
static int mx_create_event_pipe(MX *mx)
{
    if (pipe(mx->event_pipe) == -1) {
        mx_error("couldn't create event_pipe (%s).\n", strerror(errno));
        return -1;
    }
    else if (fcntl(mx->event_pipe[RD], F_SETFL, O_NONBLOCK) == -1) {
        mx_error("couldn't set O_NONBLOCK on event pipe (%s).\n", strerror(errno));
        return -1;
    }

    return 0;
}

/*
 * Start the thread that waits for incoming connection requests.
 */
static int mx_start_listener_thread(MX *mx)
{
    int r;

    r = pthread_create(&mx->listener_thread, NULL, mx_listener_thread, mx);

    if (r != 0) {
        mx_error("couldn't create reader thread (%s)\n", strerror(r));
        return -1;
    }

    return 0;
}

/*
 * Stop the thread that waits for incoming connection requests.
 */
static void mx_stop_listener_thread(MX *mx)
{
    if (mx->listener_thread == 0) {
        return;
    }

    shutdown(mx->listen_fd, SHUT_RDWR);

    pthread_join(mx->listener_thread, NULL);

    mx->listener_thread = 0;
}

/*
 * Create and return a timer struct. with the given parameters.
 */
static MX_Timer *mx_create_timer(uint32_t id, double t, void (*handler)(MX *mx,
            uint32_t id, double t, void *udata), void *udata)
{
    MX_Timer *timer = calloc(1, sizeof(*timer));

    timer->id      = id;
    timer->t       = t;
    timer->handler = handler;
    timer->udata   = udata;

    return timer;
}

/*
 * Find the timer with the given id.
 */
static MX_Timer *mx_find_timer(MX *mx, uint32_t id)
{
    MX_Timer *timer;

    for (timer = listHead(&mx->timers); timer; timer = listNext(timer)) {
        if (timer->id == id) break;
    }

    return timer;
}

/*
 * Compare two MX_Timers in p1 and p2 and return -1, 0 or 1 depending on whether
 * the time in p1 is less than, equal to or greater than the one in p2.
 */
static int mx_compare_timers(const void *p1, const void *p2)
{
    const MX_Timer *t1 = p1;
    const MX_Timer *t2 = p2;

    if (t1->t < t2->t)
        return -1;
    else if (t1->t > t2->t)
        return 1;
    else
        return 0;
}

/*
 * The timer_thread. This maintains a list of timers and sends Timer events back
 * to the main loop when one expires. Timers are added by sending them over the
 * timer_pipe.
 */
static void *mx_timer_thread(void *arg)
{
    MX *mx = arg;
    MX_Timer *timer;

    double *deadline_ptr;

    /* Wait for timer commands on the timer_queue. */

    while (1) {
        timer = listHead(&mx->timers);

        if (timer == NULL) {
            deadline_ptr = NULL;
        }
        else if (timer->t == DBL_MAX) {
            deadline_ptr = NULL;
        }
        else {
            deadline_ptr = &timer->t;
        }

        MX_Command *cmd = mx_pop_command(&mx->timer_queue, deadline_ptr);

        if (cmd == NULL) {
            if (errno == ETIMEDOUT) {
                listRemove(&mx->timers, timer);

                mx_send_pointer(mx->event_pipe[WR],
                        mx_timer_event(timer->t, timer->id, timer->handler, timer->udata));

                free(timer);
            }
            else {
                mx_send_pointer(mx->event_pipe[WR],
                        mx_error_event(-1, "mx_pop_command", errno));
                break;
            }
        }
        else if (cmd->cmd_type == MX_CT_TIMER_CREATE) {
            /* New timer on timer_pipe. Add it and re-sort. */

            if (mx_find_timer(mx, cmd->u.timer_create.id) == NULL) {
                timer = mx_create_timer(cmd->u.timer_create.id, cmd->u.timer_create.t,
                        cmd->u.timer_create.handler, cmd->u.timer_create.udata);

                listAppendTail(&mx->timers, timer);
                listSort(&mx->timers, mx_compare_timers);
            }
            else {
                mx_send_pointer(mx->event_pipe[WR],
                        mx_error_event(-1, "mxCreateTimer", EEXIST));
            }

            free(cmd);
        }
        else if (cmd->cmd_type == MX_CT_TIMER_ADJUST) {
            MX_Timer *existing_timer = mx_find_timer(mx, cmd->u.timer_adjust.id);

            if (existing_timer != NULL) {
                existing_timer->t = cmd->u.timer_adjust.t;
                listSort(&mx->timers, mx_compare_timers);
            }
            else {
                mx_send_pointer(mx->event_pipe[WR],
                        mx_error_event(-1, "mxAdjustTimer", ENOENT));
            }

            free(cmd);
        }
        else if (cmd->cmd_type == MX_CT_TIMER_DELETE) {
            MX_Timer *existing_timer = mx_find_timer(mx, cmd->u.timer_delete.id);

            if (existing_timer != NULL) {
                listRemove(&mx->timers, existing_timer);

                free(existing_timer);
            }
            else {
                mx_send_pointer(mx->event_pipe[WR],
                        mx_error_event(-1, "mxRemoveTimer", ENOENT));
            }

            free(cmd);
        }
        else if (cmd->cmd_type == MX_CT_EXIT) {
            free(cmd);

            break;
        }
        else {
            mx_send_pointer(mx->event_pipe[WR],
                        mx_error_event(-1, "read", EINVAL));

            free(cmd);

            break;
        }
    }

    while ((timer = listRemoveHead(&mx->timers)) != NULL) {
        free(timer);
    }

    return NULL;
}

static int mx_start_timer_thread(MX *mx)
{
    int r;

    r = pthread_create(&mx->timer_thread, NULL, mx_timer_thread, mx);

    if (r != 0) {
        mx_error("couldn't create reader thread (%s)\n", strerror(r));
        return -1;
    }

    return 0;
}

static void mx_stop_timer_thread(MX *mx)
{
    if (mx->timer_thread == 0) {
        return;
    }

    MX_Command *cmd = mx_create_exit_command();

    mx_push_command(&mx->timer_queue, cmd);

    pthread_join(mx->timer_thread, NULL);

    mx->timer_thread = 0;
}

/*
 * We have incoming data on component <comp>; it is <size> bytes long and is
 * contained in <data>. Process it.
 */
static void mx_handle_incoming(MX_Component *comp, const char *data, size_t data_size)
{
    bufAdd(&comp->incoming, data, data_size);

    while (bufLen(&comp->incoming) >= HEADER_SIZE) {
        MX_Await *await;

        uint32_t type;
        uint32_t version;
        uint32_t size;
        char *payload;

        strunpack(bufGet(&comp->incoming), bufLen(&comp->incoming),
                PACK_INT32, &type,
                PACK_INT32, &version,
                PACK_INT32, &size,
                END);

        if (bufLen(&comp->incoming) < HEADER_SIZE + size) {
            break;
        }

        payload = memdup(bufGet(&comp->incoming) + HEADER_SIZE, size);

        bufTrim(&comp->incoming, HEADER_SIZE + size, 0);

        /* Maybe someone is waiting for this message? First set a read/write
         * lock so we can inspect the list of awaits. */

        pthread_rwlock_wrlock(&comp->await_lock);

        for (await = listHead(&comp->awaits); await; await = listNext(await)) {
            if (await->msg_type == type) {
                listRemove(&comp->awaits, await);
                break;
            }
        }

        pthread_rwlock_unlock(&comp->await_lock);

        if (await != NULL) {            /* Someone is waiting! Pop the lock. */
            await->version = version;
            await->payload = payload;
            await->size = size;

            pthread_mutex_unlock(&await->mutex);
        }
        else {                          /* No-one waiting: deliver normally. */
            mx_send_pointer(comp->mx->event_pipe[WR],
                    mx_message_event(comp->fd, type, version, payload, size));
        }
    }
}

/*
 * A thread to read incoming messages on a file descriptor. <arg> is a pointer
 * to an MX_Component struct.
 */
static void *mx_reader_thread(void *arg)
{
    MX_Component *comp = arg;

    /* Listen for external data on comp->fd, exit when the connection on the
     * reader_pipe is lost. */

    while (1) {
        char data[9000];

        ssize_t r = read(comp->fd, data, sizeof(data));

        if (r == 0) {               /* Lost connection. */
            mx_send_pointer(comp->mx->event_pipe[WR],
                    mx_disc_event(comp->fd, "read"));

            break;
        }
        else if (r < 0) {
            mx_send_pointer(comp->mx->event_pipe[WR],
                    mx_error_event(comp->fd, "read", errno));
            break;
        }
        else {                      /* Incoming data: handle it. */
            mx_handle_incoming(comp, data, r);
        }
    }

    return NULL;
}

static int mx_start_reader_thread(MX *mx, MX_Component *comp)
{
    int r;

    r = pthread_create(&comp->reader_thread, NULL, mx_reader_thread, comp);

    if (r != 0) {
        mx_error("couldn't create reader thread (%s)\n", strerror(r));
        return -1;
    }

    return 0;
}

static void mx_stop_reader_thread(MX *mx, MX_Component *comp)
{
    if (comp->reader_thread == 0) {
        return;
    }

    shutdown(comp->fd, O_RDWR);

    pthread_join(comp->reader_thread, NULL);    /* Wait for reader to exit. */

    comp->reader_thread = 0;
}

/*
 * A thread to write outgoing messages to a file descriptor. <arg> is a pointer
 * to an MX_WriterInfo struct.
 */
static void *mx_writer_thread(void *arg)
{
    MX_Component *comp = arg;

    Buffer outgoing = { 0 };

    /* Wait for commands from the writer_queue and write data to comp->fd. */

    while (1) {
        MX_Command *cmd = mx_pop_command(&comp->writer_queue, NULL);

        if (cmd->cmd_type == MX_CT_EXIT) {
            break;
        }
        else if (cmd->cmd_type == MX_CT_WRITE) {
            bufPack(&outgoing,
                PACK_INT32, cmd->u.write.msg_type,
                PACK_INT32, cmd->u.write.version,
                PACK_INT32, cmd->u.write.size,
                PACK_RAW,   cmd->u.write.payload, cmd->u.write.size,
                END);

            tcpWrite(comp->fd, bufGet(&outgoing), bufLen(&outgoing));

            bufClear(&outgoing);
        }
        else {
            mx_error("unexpected command type in writer thread: %d (%s)\n",
                    cmd->cmd_type, cmd_enum_to_string(cmd->cmd_type));
            break;
        }
    }

    bufRewind(&outgoing);

    return NULL;
}

/*
 * Start a thread to write messages to <comp>.
 */
static int mx_start_writer_thread(MX *mx, MX_Component *comp)
{
    int r = pthread_create(&comp->writer_thread, NULL, mx_writer_thread, comp);

    if (r != 0) {
        mx_error("couldn't create writer thread (%s)\n", strerror(r));
        return -1;
    }

    return 0;
}

/*
 * Stop the thread to write messages to <comp>.
 */
static void mx_stop_writer_thread(MX *mx, MX_Component *comp)
{
    if (comp->writer_thread == 0) {
        return;
    }

    MX_Command *cmd = mx_create_exit_command();

    mx_push_command(&comp->writer_queue, cmd);

    pthread_join(comp->writer_thread, NULL);    /* Wait for writer to exit. */

    comp->writer_thread = 0;
}

/*
 * Create a new component in <mx>.
 */
static MX_Component *mx_create_component(MX *mx)
{
    MX_Component *comp = calloc(1, sizeof(*comp));

    comp->mx = mx;

    mx_init_queue(&comp->writer_queue);

    bufClear(&mx_message);

    pthread_rwlock_init(&comp->await_lock, NULL);

    return comp;
}

/*
 * Count the number of components known to <mx>. If <name> is not NULL, only
 * the components whose name begins with <name> are counted.
 */
static uint16_t mx_count_components(const MX *mx, const char *name)
{
    int component_count = paCount(&mx->components);
    int found = 0;
    int name_len = name ? strlen(name) : 0;

    for (int i = 0; i < component_count; i++) {
        MX_Component *comp = paGet(&mx->components, i);

        if (comp == NULL) {
            continue;
        }
        else if (name == NULL) {
            found++;
        }
        else if (comp->name == NULL) {
            continue;
        }
        else if (strncmp(name, comp->name, name_len) == 0) {
            found++;
        }
    }

    return found;
}

/*
 * Destroy all subscriptions by component <comp>.
 */
static void mx_destroy_component_subscriptions(MX_Component *comp)
{
    MX_Subscription *sub;

    while ((sub = mlRemoveHead(&comp->subscriptions)) != NULL) {
        mlRemove(&sub->msg->subscriptions, sub);
        free(sub);
    }
}

/*
 * Destroy all subscriptions on message <msg>.
 */
static void mx_destroy_message_subscriptions(MX_Message *msg)
{
    MX_Subscription *sub;

    while ((sub = mlRemoveHead(&msg->subscriptions)) != NULL) {
        mlRemove(&sub->comp->subscriptions, sub);
        free(sub);
    }
}

/*
 * Destroy component <comp>.
 */
static void mx_destroy_component(MX *mx, MX_Component *comp)
{
    MX_Await *await;

    bufRewind(&comp->incoming);

    pthread_rwlock_destroy(&comp->await_lock);

    /* Destroy any pending awaits. */

    while ((await = listRemoveHead(&comp->awaits)) != NULL) {
        pthread_mutex_unlock(&await->mutex);
        pthread_mutex_destroy(&await->mutex);

        if (await->payload != NULL) free(await->payload);

        free(await);
    }

    if (comp != mx->me && comp->name != NULL && mx->on_end_comp_callback) {
        mx->on_end_comp_callback(mx, comp->fd, comp->name, mx->on_end_comp_udata);
    }

    mx_stop_reader_thread(mx, comp);
    mx_stop_writer_thread(mx, comp);

    mx_destroy_component_subscriptions(comp);

    free(comp->name);
    free(comp->host);

    free(comp);
}

/*
 * Find the subscription to <msg> in <comp>.
 */
static MX_Subscription *mx_find_subscription_for_comp(MX_Message *msg, MX_Component *comp)
{
    MX_Subscription *sub;

    for (sub = mlHead(&comp->subscriptions); sub;
         sub = mlNext(&comp->subscriptions, sub)) {
        if (sub->msg == msg) break;
    }

    return sub;
}

/*
 * Find the subscription from <comp> in <msg>.
 */
static MX_Subscription *mx_find_subscription_to_msg(MX_Component *comp, MX_Message *msg)
{
    MX_Subscription *sub;

    for (sub = mlHead(&msg->subscriptions); sub;
         sub = mlNext(&msg->subscriptions, sub)) {
        if (sub->comp == comp) break;
    }

    return sub;
}

/*
 * Handle message <msg>, that came in via file descriptor <fd>.
 */
static void mx_handle_message(MX *mx, int fd,
        uint32_t type, uint32_t version, char *payload, uint32_t size)
{
    MX_Message *msg;
    MX_Subscription *sub;

    if ((msg = hashGet(&mx->message_by_type, HASH_VALUE(type))) == NULL) {
        return;
    }

    if ((sub = mx_find_subscription_for_comp(msg, mx->me)) != NULL) {
        sub->handler(mx, fd, type, version, payload, size, sub->udata);
    }
}

/*
 * Handle an MX_MT_REGISTER_REPORT (only in regular components). The message
 * came in on fd <fd> with type <type>, version <version>, and had payload
 * <payload> with size <size>.
 */
static void mx_handle_register_report(MX *mx, int fd,
        uint32_t type, uint32_t version,
        char *payload, uint32_t size, void *udata)
{
    MX_Message *msg;
    char *msg_name;

    strunpack(payload, size,
            PACK_STRING,    &msg_name,
            PACK_INT32,     &type,
            END);

    free(payload);

    if (strlen(msg_name) == 0) {        /* An anonymous message! */
        free(msg_name);
        msg_name = NULL;
    }

    msg = hashGet(&mx->message_by_type, HASH_VALUE(type));

    if (msg == NULL) {
        mx_create_message(mx, type, msg_name);

        free(msg_name);
    }
    else if (msg->msg_name == NULL && msg_name != NULL) {
        msg->msg_name = msg_name;

        hashAdd(&mx->message_by_name, msg, HASH_STRING(msg_name));
    }
    else if (msg_name != NULL) {
        free(msg_name);
    }
}

/*
 * Handle a SUBSCRIBE_UPDATE message (in all clients). This message
 * is exchanged between clients to inform each other of new
 * subscriptions.
 */
static void mx_handle_subscribe_update(MX *mx, int fd,
        uint32_t type, uint32_t version,
        char *payload, uint32_t size, void *udata)
{
    MX_Message *msg;

    MX_Component *comp = paGet(&mx->components, fd);
    MX_Subscription *sub = calloc(1, sizeof(*sub));

    strunpack(payload, size, PACK_INT32, &type, END);

    free(payload);

    msg = hashGet(&mx->message_by_type, HASH_VALUE(type));

    if (msg == NULL) {
        msg = mx_create_message(mx, type, NULL);
    }

    sub->comp = comp;
    sub->msg  = msg;

    mlAppendTail(&msg->subscriptions, sub);
    mlAppendTail(&comp->subscriptions, sub);

    if (msg->on_new_sub_callback) {
        msg->on_new_sub_callback(mx, fd, type, msg->on_new_sub_udata);
    }
}

/*
 * Handle a CANCEL_UPDATE message (only in regular components). This message is
 * exchanged between regular components to inform each other of cancelled
 * subscriptions.
 */
static void mx_handle_cancel_update(MX *mx, int fd,
        uint32_t type, uint32_t version,
        char *payload, uint32_t size, void *udata)
{
    MX_Message *msg;
    MX_Subscription *sub;

    MX_Component *comp = paGet(&mx->components, fd);

    strunpack(payload, size, PACK_INT32, &type, END);

    free(payload);

    if ((msg = hashGet(&mx->message_by_type, HASH_VALUE(type))) == NULL) {
        return;
    }

    if ((sub = mx_find_subscription_to_msg(comp, msg)) == NULL) {
        return;
    }

    mlRemove(&msg->subscriptions, sub);
    mlRemove(&comp->subscriptions, sub);

    if (msg->on_end_sub_callback) {
        msg->on_end_sub_callback(mx, fd, type, msg->on_end_sub_udata);
    }

    free(sub);
}

/*
 * Handle a HELLO_REPORT message (only in regular components). This message is
 * sent by the master to inform a recently connected component of the already
 * existing components in the system.
 */
static void mx_handle_hello_report(MX *mx, int fd,
        uint32_t type, uint32_t version,
        char *payload, uint32_t size, void *udata)
{
    char *name;
    char *host;
    uint16_t port;
    uint16_t id;

    MX_Subscription *sub;
    MX_Component *comp;

    strunpack(payload, size,
            PACK_STRING,    &name,
            PACK_INT16,     &id,
            PACK_STRING,    &host,
            PACK_INT16,     &port,
            END);

    free(payload);

    /* Create component data and connect to it. */

    fd = tcpConnect(host, port);

    if (fd == -1) {
        mx_error("could not connect to component %s at %s:%d (%s).\n",
                name, host, port, strerror(errno));
        mxShutdown(mx);
        return;
    }

    comp = mx_create_component(mx);

    comp->name = name;
    comp->host = host;
    comp->port = port;
    comp->fd   = fd;
    comp->id   = id;

    paSet(&mx->components, comp->fd, comp);

    mx_start_reader_thread(mx, comp);
    mx_start_writer_thread(mx, comp);

    /* Tell it who we are... */

    mx_pack(comp, MX_MT_HELLO_UPDATE, 0,
            PACK_STRING,    mx->me->name,
            PACK_INT16,     mx->me->id,
            PACK_INT16,     mx->me->port,
            END);

    /* And inform it of all of our subscriptions. */

    for (sub = mlHead(&mx->me->subscriptions); sub;
         sub = mlNext(&mx->me->subscriptions, sub)) {
        mx_pack(comp, MX_MT_SUBSCRIBE_UPDATE, 0,
                PACK_INT32, sub->msg->msg_type,
                END);
    }

    if (mx->on_new_comp_callback) {
        mx->on_new_comp_callback(mx, comp->fd, name, mx->on_new_comp_udata);
    }
}

/*
 * Handle a HELLO_UPDATE message (only in regular components). This message is
 * sent by a new component to existing components as a way of introducing
 * themselves.
 */
static void mx_handle_hello_update(MX *mx, int fd,
        uint32_t type, uint32_t version,
        char *payload, uint32_t size, void *udata)
{
    MX_Subscription *sub;

    char *name;
    uint16_t port;
    uint16_t id;

    MX_Component *comp = paGet(&mx->components, fd);

    strunpack(payload, size,
            PACK_STRING, &name,
            PACK_INT16,  &id,
            PACK_INT16,  &port,
            END);

    free(payload);

    dbgAssert(stderr, comp->name == NULL,
            "Expected comp->name to be NULL instead of \"%s\"\n", comp->name);

    comp->name = name;
    comp->host = strdup(netPeerHost(fd));
    comp->port = port;
    comp->id   = id;

    /* Inform the new component of all of my subscriptions. */

    for (sub = mlHead(&mx->me->subscriptions); sub;
         sub = mlNext(&mx->me->subscriptions, sub)) {

        mx_pack(comp, MX_MT_SUBSCRIBE_UPDATE, 0,
                PACK_INT32, sub->msg->msg_type,
                END);
    }

    if (mx->on_new_comp_callback) {
        mx->on_new_comp_callback(mx, comp->fd, name, mx->on_new_comp_udata);
    }
}

/*
 * Handle a HELLO_REQUEST message (only in the master component). This message
 * is sent by new components to the master to introduce themselves.
 */
static void mx_handle_hello_request(MX *mx, int fd,
        uint32_t type, uint32_t version,
        char *payload, uint32_t size, void *udata)
{
    char *name;
    uint16_t port;
    MX_Subscription *sub;

    strunpack(payload, size,
            PACK_STRING,    &name,
            PACK_INT16,     &port,
            END);

    free(payload);

    MX_Component *comp = paGet(&mx->components, fd);

    dbgAssert(stderr, comp != NULL,
            "HELLO request from unconnected component?!\n");

    dbgAssert(stderr, comp->name == NULL,
            "Expected comp->name to be NULL instead of \"%s\"\n", comp->name);

    int name_len = asprintf(&comp->name, "%s/%u",
            name, mx_count_components(mx, name) + 1);

    dbgAssert(stderr, name_len != -1, "Could not create component name.\n");

    comp->id   = mx_count_components(mx, NULL);
    comp->host = strdup(netPeerHost(fd));
    comp->port = port;

    /* Tell it my name (which may be different from "master"), its new id and
     * its new name. */

    mx_pack(comp, MX_MT_HELLO_REPLY, 0,
            PACK_STRING,    mx->me->name,
            PACK_INT16,     comp->id,
            PACK_STRING,    comp->name,
            END);

    /* Inform the new component of alle existing components. */

    for (fd = 0; fd < paCount(&mx->components); fd++) {
        MX_Component *existing = paGet(&mx->components, fd);

        if (existing == NULL || existing == comp || existing->name == NULL)
            continue;

        mx_pack(comp, MX_MT_HELLO_REPORT, 0,
                PACK_STRING,    existing->name,
                PACK_INT16,     existing->id,
                PACK_STRING,    existing->host,
                PACK_INT16,     existing->port,
                END);
    }

    /* Inform the new component of all registered messages. */

    for (type = NUM_MX_MESSAGES; type < mx->next_message_type; type++) {
        MX_Message *msg = hashGet(&mx->message_by_type, HASH_VALUE(type));

        mx_pack(comp, MX_MT_REGISTER_REPORT, 0,
                PACK_STRING,    msg->msg_name ? msg->msg_name : "",
                PACK_INT32,     msg->msg_type,
                END);
    }

    /* Inform the new component of my subscriptions. */

    for (sub = mlHead(&mx->me->subscriptions); sub;
         sub = mlNext(&mx->me->subscriptions, sub)) {

        mx_pack(comp, MX_MT_SUBSCRIBE_UPDATE, 0,
                PACK_INT32, sub->msg->msg_type,
                END);
    }

    if (mx->on_new_comp_callback) {
        mx->on_new_comp_callback(mx, comp->fd, name, mx->on_new_comp_udata);
    }
}

/*
 * Handle a REGISTER_REQUEST (only in the master component). This message is
 * sent from a component to the master to register a message type.
 */
static void mx_handle_register_request(MX *mx, int fd,
        uint32_t type, uint32_t version,
        char *payload, uint32_t size, void *udata)
{
    char *msg_name = NULL;
    MX_Message *msg = NULL;

    MX_Component *comp = paGet(&mx->components, fd);

    /* Get the name of the message. */

    strunpack(payload, size,
            PACK_STRING, &msg_name,
            END);

    free(payload);

    if (strlen(msg_name) == 0) {        /* An anonymous message! */
        free(msg_name);
        msg_name = NULL;
    }

    if (msg_name == NULL) {             /* Always allocate anon messages. */
        msg = mx_create_message(mx, mx->next_message_type, msg_name);

        mx_broadcast_new_message(mx, msg, comp);
    }
    else {                              /* Else try to find existing... */
        msg = hashGet(&mx->message_by_name, HASH_STRING(msg_name));

        if (msg == NULL) {              /* Or create a new message. */
            msg = mx_create_message(mx, mx->next_message_type, msg_name);

            mx_broadcast_new_message(mx, msg, comp);
        }
    }

    if (msg_name != NULL) free(msg_name);

    /* Send reply. */

    mx_pack(comp, MX_MT_REGISTER_REPLY, version,
            PACK_INT32,     msg->msg_type,
            END);
}

/*
 * Handle a QUIT_REQUEST message (only in the master).
 */
static void mx_handle_quit_request(MX *mx, int client_fd,
        uint32_t type, uint32_t version,
        char *payload, uint32_t size, void *udata)
{
    free(payload);

    mxShutdown(mx);
}

/*
 * Handle a new connection on <fd>.
 */
static void mx_handle_connect(MX *mx, int fd)
{
    MX_Component *comp = mx_create_component(mx);

    comp->fd = fd;

    paSet(&mx->components, fd, comp);

    mx_start_reader_thread(mx, comp);
    mx_start_writer_thread(mx, comp);
}

/*
 * Handle a disconnect event on <fd>, triggered at <whence>.
 */
static void mx_handle_disconnect(MX *mx, int fd, char *whence)
{
    MX_Component *comp = paGet(&mx->components, fd);

    free(whence);

    if (comp == mx->master) {
        mx_notice("lost connection with master, shutting down.\n");
        mxShutdown(mx);
    }
    else if (comp != NULL) {
        paDrop(&mx->components, fd);

        mx_destroy_component(mx, comp);
    }
}

/*
 * Subscribe to messages of type <type>. <handler> will be called for all
 * incoming messages of this type, passing in the same <udata> that is passed in
 * to this function. Returns <0 on errors, >0 on notices and 0 otherwise.
 */
static int mx_subscribe(MX *mx, uint32_t type,
        void (*handler)(MX *mx, int fd, uint32_t type, uint32_t version,
            char *payload, uint32_t size, void *udata),
        void *udata)
{
    int fd, r = 0;

    MX_Message *msg;
    MX_Subscription *sub;

    if ((msg = hashGet(&mx->message_by_type, HASH_VALUE(type))) == NULL) {
        mx_notice("Subscribing to unknown message type %d. "
                  "Adding message type.\n", type);

        msg = mx_create_message(mx, type, NULL);

        r = 1;
    }

    if ((sub = mx_find_subscription_for_comp(msg, mx->me)) != NULL) {
        mx_notice("mxSubscribe for message type %d (%s), "
                  "which I'm already subscribed to. Replacing callback.\n",
                  type, mxMessageName(mx, type));

        sub->handler = handler;
        sub->udata = udata;

        return 2;
    }

    sub = calloc(1, sizeof(*sub));

    sub->msg = msg;
    sub->comp = mx->me;

    sub->handler = handler;
    sub->udata = udata;

    mlAppendTail(&msg->subscriptions, sub);
    mlAppendTail(&mx->me->subscriptions, sub);

    for (fd = 0; fd < paCount(&mx->components); fd++) {
        MX_Component *comp = paGet(&mx->components, fd);

        if (comp == NULL) continue;

        mx_pack(comp, MX_MT_SUBSCRIBE_UPDATE, 0, PACK_INT32, type, END);
    }

    return r;
}

/*
 * Cancel our subscription to messages of type <type> that calls <handler>.
 * Returns <0 on errors, >0 on notices and 0 otherwise.
 */
static int mx_cancel(MX *mx, uint32_t type)
{
    int fd;

    MX_Message *msg;
    MX_Subscription *sub;

    if ((msg = hashGet(&mx->message_by_type, HASH_VALUE(type))) == NULL) {
        mx_notice("mxCancel for unknown message type %d. Ignored\n", type);
        return 1;
    }

    if ((sub = mx_find_subscription_for_comp(msg, mx->me)) == NULL) {
        mx_notice("mxCancel for message type %d (%s), "
                  "which I'm not subscribed to. Ignored.\n",
                  type, mxMessageName(mx, type));
        return 1;
    }

    mlRemove(&sub->msg->subscriptions, sub);
    mlRemove(&mx->me->subscriptions, sub);

    free(sub);

    for (fd = 0; fd < paCount(&mx->components); fd++) {
        MX_Component *comp = paGet(&mx->components, fd);

        if (comp == NULL) continue;

        mx_pack(comp, MX_MT_CANCEL_UPDATE, 0, PACK_INT32, type, END);
    }

    return 0;
}

/*
 * Return the mx_name to use if <mx_name> was given to mxClient() or mxMaster().
 * If it is a valid name (i.e. not NULL) use it. Otherwise use the environment
 * variable MX_NAME if it is set. Otherwise use the environment variable USER
 * (the user's login name) if it is set. Otherwise give up and return NULL.
 */
const char *mxEffectiveName(const char *mx_name)
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
 * Return the listen port that the master component will use for mx name
 * <mx_name>.
 */
uint16_t mxEffectivePort(const char *mx_name)
{
    const char *p;
    int hash = 0;

    mx_name = mxEffectiveName(mx_name);

    for (p = mx_name; *p != '\0'; p++) {
        hash += *p * 307;
    }

    return MIN_PORT + (hash % (MAX_PORT - MIN_PORT + 1));
}

/*
 * Return the mx_host to use if <mx_host> was given to mxClient() or mxMaster().
 * If it is a valid name (i.e. not NULL) use it. Otherwise use the environment
 * variable MX_HOST if it is set. Otherwise simply use "localhost".
 */
const char *mxEffectiveHost(const char *mx_host)
{
    if (mx_host != NULL)
        return mx_host;
    else if ((mx_host = getenv("MX_HOST")) != NULL)
        return mx_host;
    else
        return "localhost";
}

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
int mxOption(char short_name, const char *long_name, int *argc, char *argv[], char **argument)
{
    int i, new_argc = 1;

    int len_long_name = strlen(long_name);

    char * const not_found = (char *) -1;

    char *full_short_name = malloc(3);
    char *full_long_name = malloc(len_long_name + 3);

    sprintf(full_short_name, "-%c", short_name);
    sprintf(full_long_name, "--%s", long_name);

    int len_full_short_name = 2;
    int len_full_long_name = len_long_name + 2;

    for (i = 1; i < *argc; i++) {
        char *my_arg = not_found;

        if (strcmp(argv[i], full_short_name) == 0) {
            my_arg = argv[++i];
        }
        else if (strncmp(argv[i], full_short_name, len_full_short_name) == 0) {
            my_arg = argv[i] + len_full_short_name;
        }
        else if (strcmp(argv[i], full_long_name) == 0) {
            my_arg = argv[++i];
        }
        else if (strncmp(argv[i], full_long_name, len_full_long_name) == 0 &&
                 argv[i][len_full_long_name] == '=') {
            my_arg = argv[i] + len_full_long_name + 1;
        }

        if (my_arg == not_found) {
            argv[new_argc++] = argv[i];
        }
        else if (my_arg == NULL || my_arg[0] == '-') {
            mx_error("Missing argument for option %s.\n", argv[i-1]);
            return -1;
        }
        else {
            *argument = my_arg;
        }
    }

    *argc = new_argc;

    return 0;
}

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
 * no communication threads have been started yet (use mx_begin() for this).
 */
static MX *mx_create_client(const char *mx_host, const char *mx_name, const char *my_name)
{
    uint16_t mx_port;

    if (my_name == NULL) {
        mx_error("<my_name> can not be NULL in call to mx_create_client (%s).\n",
                strerror(EINVAL));
        return NULL;
    }

    mx_host = mxEffectiveHost(mx_host);
    mx_name = mxEffectiveName(mx_name);
    mx_port = mxEffectivePort(mx_name);

    if (mx_name == NULL) {
        mx_error("couldn't determine MX name.\n");
        return NULL;
    }

    MX *mx = calloc(1, sizeof(*mx));

    if ((mx->listen_fd = tcpListen(NULL, 0)) == -1) {
        mx_error("couldn't open a listen socket (%s).\n", strerror(errno));
        free(mx);
        return NULL;
    }

    if ((mx->master = mx_create_component(mx)) == NULL) {
        mx_error("couldn't create master component for \"%s\".\n", mx_name);
        free(mx);
        return NULL;
    }

    if ((mx->me = mx_create_component(mx)) == NULL) {
        mx_error("couldn't create own component for \"%s\".\n", mx_name);
        mx_destroy_component(mx, mx->master);
        free(mx);
        return NULL;
    }

    mx_init_queue(&mx->timer_queue);

    mx->master->host = strdup(mx_host);
    mx->master->port = mx_port;

    mx->me->name = strdup(my_name);
    mx->me->port = netLocalPort(mx->listen_fd),

    mx->mx_name = strdup(mx_name);

    return mx;
}

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
 * no communication threads have been started yet (use mx_begin() for this).
 */
static MX *mx_create_master(const char *mx_name, const char *my_name)
{
    uint16_t mx_port;

    if (my_name == NULL) {
        my_name = "master";
    }

    mx_name = mxEffectiveName(mx_name);
    mx_port = mxEffectivePort(mx_name);

    if (mx_name == NULL) {
        mx_error("couldn't determine MX name.\n");
        return NULL;
    }

    MX *mx = calloc(1, sizeof(*mx));

    if ((mx->listen_fd = tcpListen(NULL, mx_port)) == -1) {
        mx_error("couldn't open listen socket on port %d (%s)\n",
                mx_port, strerror(errno));
        free(mx);
        return NULL;
    }

    if ((mx->me = mx->master = mx_create_component(mx)) == NULL) {
        mx_error("couldn't create own component for \"%s\".\n", mx_name);
        close(mx->listen_fd);
        free(mx);
        return NULL;
    }

    mx->me->host = strdup("localhost");
    mx->me->name = strdup(my_name);
    mx->me->port = mx_port;
    mx->me->id   = 0;

    mx->mx_name = strdup(mx_name);

    return mx;
}

/*
 * Begin running the threads that listen for connection and timer events.
 */
static int mx_begin(MX *mx)
{
    mx_create_message(mx, MX_MT_QUIT_REQUEST, "QuitRequest");
    mx_create_message(mx, MX_MT_HELLO_REQUEST, "HelloRequest");
    mx_create_message(mx, MX_MT_HELLO_REPLY, "HelloReply");
    mx_create_message(mx, MX_MT_HELLO_REPORT, "HelloReport");
    mx_create_message(mx, MX_MT_HELLO_UPDATE, "HelloUpdate");
    mx_create_message(mx, MX_MT_REGISTER_REQUEST, "RegisterRequest");
    mx_create_message(mx, MX_MT_REGISTER_REPORT, "RegisterReport");
    mx_create_message(mx, MX_MT_REGISTER_REPLY, "RegisterReply");
    mx_create_message(mx, MX_MT_SUBSCRIBE_UPDATE, "SubscribeUpdate");
    mx_create_message(mx, MX_MT_CANCEL_UPDATE, "CancelUpdate");

    mx_create_event_pipe(mx);

    mx_start_timer_thread(mx);
    mx_start_listener_thread(mx);

    if (mx->me == mx->master) {     /* Running as master */
        mx_subscribe(mx, MX_MT_QUIT_REQUEST, mx_handle_quit_request, NULL);
        mx_subscribe(mx, MX_MT_HELLO_REQUEST, mx_handle_hello_request, NULL);
        mx_subscribe(mx, MX_MT_REGISTER_REQUEST, mx_handle_register_request, NULL);
        mx_subscribe(mx, MX_MT_SUBSCRIBE_UPDATE, mx_handle_subscribe_update, NULL);
        mx_subscribe(mx, MX_MT_CANCEL_UPDATE, mx_handle_cancel_update, NULL);
    }
    else {                          /* Running as client */
        int r;
        uint32_t reply_version, reply_size;
        char *reply_payload;

        if ((mx->master->fd = tcpConnect(mx->master->host, mx->master->port)) < 0) {
            mx_error("couldn't connect to master for \"%s\" at %s:%d (%s).\n",
                    mx->mx_name, mx->master->host, mx->master->port, strerror(errno));
            return -1;
        }

        paSet(&mx->components, mx->master->fd, mx->master);

        mx_start_reader_thread(mx, mx->master);
        mx_start_writer_thread(mx, mx->master);

        r = mx_pack_and_wait(mx->master, 5,
                MX_MT_HELLO_REPLY, &reply_version, &reply_payload, &reply_size,
                MX_MT_HELLO_REQUEST, 0,
                PACK_STRING,    mx->me->name,
                PACK_INT16,     mx->me->port,
                END);

        if (r != 0) {
            mx_error("%s while waiting for HelloReply.\n",
                    r == 1 ? "timeout" : "error");
            return -1;
        }

        strunpack(reply_payload, reply_size,
                PACK_STRING, &mx->master->name,
                PACK_INT16,  &mx->me->id,
                PACK_STRING, &mx->me->name,
                END);

        free(reply_payload);

        mx_subscribe(mx, MX_MT_HELLO_REPORT, mx_handle_hello_report, NULL);
        mx_subscribe(mx, MX_MT_HELLO_UPDATE, mx_handle_hello_update, NULL);
        mx_subscribe(mx, MX_MT_REGISTER_REPORT, mx_handle_register_report, NULL);
        mx_subscribe(mx, MX_MT_SUBSCRIBE_UPDATE, mx_handle_subscribe_update, NULL);
        mx_subscribe(mx, MX_MT_CANCEL_UPDATE, mx_handle_cancel_update, NULL);
    }

    return 0;
}

/*
 * Create and return an MX struct that will act as a master for the Message
 * Exchange with name <mx_name>, running on the local host.
 *
 * If <mx_name> is NULL, the environment variable MX_NAME is used instead. If
 * that doesn't exist, the environment variable USER is used. If that doesn't
 * exist either, the function fails and NULL is returned. If <my_name> is NULL,
 * "master" is used.
 *
 * If <background> is true, the master component will be put into the background
 * after the listen port for this master component has been opened. This means
 * that any additional components started after the master (in a shell script,
 * for instance) will find a listen port waiting for them.
 *
 * This "backgrounding" is done using a fork() system call, which means that any
 * threads started before calling this function (if any) will not survive. So if
 * you want to background the master component and also start additional
 * threads, do the latter *after* calling this function.
 */
MX *mxMaster(const char *mx_name, const char *my_name, bool background)
{
    int r;
    pid_t pid;

    MX *mx = mx_create_master(mx_name, my_name);

    if (mx == NULL) {
        mx_error("couldn't create mx (%s).\n", strerror(errno));
        return NULL;
    }
    else if (background && (pid = fork()) == -1) {
        mx_error("fork() failed (%s).\n", strerror(errno));

        mxShutdown(mx);
        mxDestroy(mx);

        return NULL;
    }
    else if (background && pid != 0) {
        exit(0);
    }
    else if ((r = mx_begin(mx)) != 0) {
        mxShutdown(mx);
        mxDestroy(mx);

        return NULL;
    }

    return mx;
}

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
MX *mxClient(const char *mx_host, const char *mx_name, const char *my_name)
{
    int r;

    MX *mx = mx_create_client(mx_host, mx_name, my_name);

    if (mx == NULL) {
        mx_error("couldn't create mx (%s).\n", strerror(errno));

        return NULL;
    }
    else if ((r = mx_begin(mx)) != 0) {
        mxShutdown(mx);
        mxDestroy(mx);

        return NULL;
    }

    return mx;
}

/*
 * Return the file descriptor on which all events associated with <mx> arrive.
 */
int mxConnectionNumber(MX *mx)
{
    return mx->event_pipe[RD];
}

/*
 * Process any pending events associated with <mx>. Returns -1 if an error
 * occurred, 1 if event processing has finished normally and 0 if no more events
 * are forthcoming (so there's no sense in waiting for them anymore).
 */
int mxProcessEvents(MX *mx)
{
    MX_Event *evt;

    int r;

    while (1) {
        if (mx->shutting_down) {
            return 0;
        }

        r = read(mx->event_pipe[RD], &evt, sizeof(evt));

        if (r == 0) {
            return mx_severity == MX_ERROR ? -1 : 0;
        }
        else if (r < 0) {
            if (errno == EAGAIN) {
                return 1;
            }
            else {
                mx_notice("read returned %d (%s)\n", r, strerror(errno));
                return -1;
            }
        }

        switch(evt->evt_type) {
        case MX_ET_CONN:
            mx_handle_connect(mx, evt->u.conn.fd);
            break;
        case MX_ET_DISC:
            mx_handle_disconnect(mx, evt->u.disc.fd, evt->u.disc.whence);
            break;
        case MX_ET_MSG:
            mx_handle_message(mx, evt->u.msg.fd,
                    evt->u.msg.msg_type, evt->u.msg.version,
                    evt->u.msg.payload, evt->u.msg.size);
            break;
        case MX_ET_TIMER:
            evt->u.timer.handler(mx, evt->u.timer.id, evt->u.timer.t, evt->u.timer.udata);
            break;
        case MX_ET_READ:
            break;
        case MX_ET_ERR:
            mx_notice("error event: %s (%d) in %s.\n",
                    strerror(evt->u.err.error), evt->u.err.error,
                    evt->u.err.whence);
            free(evt->u.err.whence);
            break;
        default:
            mx_notice("unexpected event type (%d)\n", evt->evt_type);
            break;
        }

        free(evt);
    }
}

/*
 * Loop while listening for and handling events. Returns -1 if an error occurred
 * or 0 if mxShutdown was called.
 */
int mxRun(MX *mx)
{
    struct pollfd poll_fd = { mx->event_pipe[RD], POLLIN, 0 };

    while (1) {
        int r = poll(&poll_fd, 1, -1);

        if (r < 0) {
            mx_error("poll() failed: %s\n", strerror(errno));

            return r;
        }

        r = mxProcessEvents(mx);

        if (r != 1) return(r);
    }
}

/*
 * Return the name of the local component.
 */
const char *mxMyName(const MX *mx)
{
    return mx->me->name;
}

/*
 * Return the ID of the local component.
 */
uint16_t mxMyID(const MX *mx)
{
    return mx->me->id;
}

/*
 * Return the current MX name.
 */
const char *mxName(const MX *mx)
{
    return mx->mx_name;
}

/*
 * Return the current MX host, i.e. the host where the master component is
 * running.
 */
const char *mxHost(const MX *mx)
{
    return mx->master->host;
}

/*
 * Return the current MX port, i.e. the port on which the master listens.
 */
uint16_t mxPort(const MX *mx)
{
    return mx->master->port;
}

/*
 * Register the message named <msg_name>. Returns the associated message type
 * id.
 */
uint32_t mxRegister(MX *mx, const char *msg_name)
{
    MX_Message *msg;

    if (msg_name != NULL &&
            (msg = hashGet(&mx->message_by_name, HASH_STRING(msg_name))) != NULL) {
        return msg->msg_type;
    }
    else if (mx->me == mx->master) {
        msg = mx_create_message(mx, mx->next_message_type, msg_name);

        mx_broadcast_new_message(mx, msg, NULL);

        return msg->msg_type;
    }
    else {
        int r;
        char *reply_payload = NULL;
        uint32_t reply_size, reply_version, msg_type;

        r = mx_pack_and_wait(mx->master, 5,
                MX_MT_REGISTER_REPLY, &reply_version,
                &reply_payload, &reply_size,
                MX_MT_REGISTER_REQUEST, 0,
                PACK_STRING, msg_name ? msg_name : "",
                END);

        if (r != 0) {
            mx_error("%s while waiting for RegisterReply.\n",
                    r == 1 ? "timeout" : "error");
            mxShutdown(mx);
            return 0;
        }
        else {
            strunpack(reply_payload, reply_size,
                    PACK_INT32, &msg_type,
                    END);

            free(reply_payload);

            msg = hashGet(&mx->message_by_type, HASH_VALUE(msg_type));

            if (msg == NULL) {
                msg = mx_create_message(mx, msg_type, msg_name);
            }
            else if (msg_name != NULL) {
                msg->msg_name = strdup(msg_name);

                hashAdd(&mx->message_by_name, msg, HASH_STRING(msg_name));
            }

            return msg->msg_type;
        }
    }
}

/*
 * Returns the name of message type <type>.
 */
const char *mxMessageName(MX *mx, uint32_t type)
{
    MX_Message *msg = hashGet(&mx->message_by_type, HASH_VALUE(type));

    return msg == NULL ? NULL : msg->msg_name;
}

/*
 * Returns the name of the component connected on fd <fd>.
 */
const char *mxComponentName(MX *mx, int fd)
{
    MX_Component *comp = paGet(&mx->components, fd);

    if (comp == NULL)
        return NULL;
    else
        return comp->name;
}

/*
 * Subscribe to messages of type <type>. <handler> will be called for all
 * incoming messages of this type, passing in the same <udata> that is passed in
 * to this function. Returns <0 on errors, >0 on notices and 0 otherwise. Check
 * mxError() when return value is not 0.
 */
int mxSubscribe(MX *mx, uint32_t type,
        void (*handler)(MX *mx, int fd, uint32_t type, uint32_t version,
            char *payload, uint32_t size, void *udata),
        void *udata)
{
    if (type < NUM_MX_MESSAGES) {
        mx_error("Illegal message type %d in mxSubscribe.\n", type);
        return -1;
    }
    else {
        return mx_subscribe(mx, type, handler, udata);
    }
}

/*
 * Cancel our subscription to messages of type <type> that calls <handler>.
 * Returns <0 on errors, >0 on notices and 0 otherwise. Check mxError() when
 * return value is not 0.
 */
int mxCancel(MX *mx, uint32_t type)
{
    if (type < NUM_MX_MESSAGES) {
        mx_error("Illegal message type %d in mxCancel.\n", type);
        return -1;
    }
    else {
        return mx_cancel(mx, type);
    }
}

/*
 * Call <handler> for new subscribers to message type <type>, passing in the
 * same <udata> that was passed in here.
 */
void mxOnNewSubscriber(MX *mx, uint32_t type,
        void (*handler)(MX *mx, int fd, uint32_t type, void *udata),
        void *udata)
{
    MX_Message *msg = hashGet(&mx->message_by_type, HASH_VALUE(type));

    if (msg == NULL) {
        msg = mx_create_message(mx, type, NULL);
    }

    msg->on_new_sub_callback = handler;
    msg->on_new_sub_udata = udata;
}

/*
 * Call <handler> when a subscriber cancels their subscription to messages of
 * type <type>.
 */
void mxOnEndSubscriber(MX *mx, uint32_t type,
        void (*handler)(MX *mx, int fd, uint32_t type, void *udata),
        void *udata)
{
    MX_Message *msg = hashGet(&mx->message_by_type, HASH_VALUE(type));

    if (msg == NULL) {
        msg = mx_create_message(mx, type, NULL);
    }

    msg->on_end_sub_callback = handler;
    msg->on_end_sub_udata = udata;
}

/*
 * Call <handler> when a new component reports in. <handler> is called with the
 * file descriptor through which we're connected to the new component in <fd>,
 * and its name in <name>.
 */
void mxOnNewComponent(MX *mx,
        void (*handler)(MX *mx, int fd, const char *name, void *udata),
        void *udata)
{
    int fd;

    mx->on_new_comp_callback = handler;
    mx->on_new_comp_udata = udata;

    for (fd = 0; fd < paCount(&mx->components); fd++) {
        MX_Component *comp = paGet(&mx->components, fd);

        if (comp == NULL || comp->name == NULL) continue;

        handler(mx, fd, comp->name, udata);
    }
}

/*
 * Call <handler> when connection with a component is lost. <handler> is called
 * with the file descriptor through which we were connected to the new component
 * in <fd>, and its name in <name>.
 */
void mxOnEndComponent(MX *mx,
        void (*handler)(MX *mx, int fd, const char *name, void *udata),
        void *udata)
{
    mx->on_end_comp_callback = handler;
    mx->on_end_comp_udata = udata;
}

/*
 * Call <handler> when a new message type is registered.
 */
void mxOnNewMessage(MX *mx,
        void (*handler)(MX *mx, uint32_t type, const char *name, void *udata),
        void *udata)
{
    uint32_t type = 0;

    mx->on_register_callback = handler;
    mx->on_register_udata = udata;

    for (type = NUM_MX_MESSAGES; type < mx->next_message_type; type++) {
        MX_Message *msg = hashGet(&mx->message_by_type, HASH_VALUE(type));

        handler(mx, msg->msg_type, msg->msg_name, udata);
    }
}

/*
 * Send a message of type <type> to file descriptor <fd>.
 */
void mxSend(MX *mx, int fd, uint32_t type, uint32_t version, const void *payload, uint32_t size)
{
    MX_Component *comp = paGet(&mx->components, fd);

    mx_send(comp, type, version, payload, size);
}

/*
 * Write a message of type <type> with version <version> to file descriptor <fd>
 * over message exchange <mx>. The payload of the message is constructed using
 * the PACK_* method as described in libjvs/utils.h.
 */
void mxPackAndSend(MX *mx, int fd, uint32_t type, uint32_t version, ...)
{
    va_list ap;

    va_start(ap, version);
    mxVaPackAndSend(mx, fd, type, version, ap);
    va_end(ap);
}

/*
 * Write a message of type <type> with version <version> to file descriptor <fd>
 * over message exchange <mx>. The payload of the message is constructed using
 * the PACK_* arguments contained in <ap>, as described in libjvs/utils.h.
 */
void mxVaPackAndSend(MX *mx, int fd, uint32_t type, uint32_t version, va_list ap)
{
    char *payload;

    int size = vastrpack(&payload, ap);

    mxSend(mx, fd, type, version, payload, size);

    free(payload);
}

/*
 * Broadcast a message with type <type>, version <version> and payload <payload>
 * with size <size> to all subscribers of this message type.
 */
void mxBroadcast(MX *mx, uint32_t type, uint32_t version, const void *payload, uint32_t size)
{
    MX_Subscription *sub;
    MX_Message *msg = hashGet(&mx->message_by_type, HASH_VALUE(type));

    for (sub = mlHead(&msg->subscriptions); sub;
         sub = mlNext(&msg->subscriptions, sub)) {
        mx_send(sub->comp, type, version, payload, size);
    }
}

/*
 * Broadcast a message with type <type> and version <version> to all subscribers
 * of this message type. The contents of the message are set using the
 * "astrpack" interface from libjvs/utils.h.
 */
void mxPackAndBroadcast(MX *mx, uint32_t type, uint32_t version, ...)
{
    va_list ap;

    va_start(ap, version);
    mxVaPackAndBroadcast(mx, type, version, ap);
    va_end(ap);
}

/*
 * Broadcast a message with type <type> and version <version> to all subscribers
 * of this message type. The contents of the message are set using the
 * "vastrpack" interface from libjvs/utils.h.
 */
void mxVaPackAndBroadcast(MX *mx, uint32_t type, uint32_t version, va_list ap)
{
    MX_Subscription *sub;
    char *payload;

    MX_Message *msg = hashGet(&mx->message_by_type, HASH_VALUE(type));

    int size = vastrpack(&payload, ap);

    for (sub = mlHead(&msg->subscriptions); sub;
         sub = mlNext(&msg->subscriptions, sub)) {
        mx_send(sub->comp, type, version, payload, size);
    }

    free(payload);
}

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
        uint32_t type, uint32_t *version, char **payload, uint32_t *size)
{
    int r;
    struct timespec ts;

    MX_Component *comp = paGet(&mx->components, fd);

    if (comp == NULL) {
        mx_error("file descriptor not connected to a component (%s).\n",
                strerror(EINVAL));
        return -1;
    }

    MX_Await *await = mx_add_await(comp, type);

    double_to_timespec(mxNow() + timeout, &ts);

    r = pthread_mutex_timedlock(&await->mutex, &ts);

    if (r == 0) {
        *version = await->version;
        *payload = await->payload;
        *size = await->size;
    }

    pthread_mutex_unlock(&await->mutex);
    pthread_mutex_destroy(&await->mutex);

    free(await->payload);
    free(await);

    return (r == 0);
}

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
        uint32_t request_type, uint32_t request_version, ...)
{
    int r;

    va_list ap;

    va_start(ap, request_version);
    r = mxVaPackAndWait(mx, fd, timeout,
            reply_type, reply_version, reply_payload, reply_size,
            request_type, request_version, ap);
    va_end(ap);

    return r;
}

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
        uint32_t request_type, uint32_t request_version, va_list ap)
{
    char *request_payload;
    int r, request_size = vastrpack(&request_payload, ap);

    r = mxSendAndWait(mx, fd, timeout,
            reply_type, reply_version, reply_payload, reply_size,
            request_type, request_version, request_payload, request_size);

    free(request_payload);

    return r;
}

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
        const char *request_payload, uint32_t request_size)
{
    MX_Component *comp = paGet(&mx->components, fd);

    if (comp == NULL) {
        mx_error("file descriptor not connected to a component (%s).\n",
                strerror(EINVAL));
        return -1;
    }

    return mx_send_and_wait(comp, timeout,
            reply_type, reply_version,
            reply_payload, reply_size,
            request_type, request_version,
            request_payload, request_size);
}

/*
 * Create a timer that will call <handler> at time <t> (seconds since the UNIX
 * epoch). In future calls to mxAdjustTimer and mxRemoveTimer this timer will be
 * identified by <id>. When calling <handler>, the same pointer <udata> given
 * here will be passed back.
 */
void mxCreateTimer(MX *mx, uint32_t id, double t,
        void (*handler)(MX *mx, uint32_t id, double t, void *udata),
        void *udata)
{
    MX_Command *cmd = calloc(1, sizeof(*cmd));

    cmd->cmd_type = MX_CT_TIMER_CREATE;

    cmd->u.timer_create.id      = id;
    cmd->u.timer_create.t       = t;
    cmd->u.timer_create.handler = handler;
    cmd->u.timer_create.udata   = udata;

    mx_push_command(&mx->timer_queue, cmd);
}

/*
 * Adjust the time of the timer with id <id> to <t>.
 */
void mxAdjustTimer(MX *mx, uint32_t id, double t)
{
    MX_Command *cmd = calloc(1, sizeof(*cmd));

    cmd->cmd_type = MX_CT_TIMER_ADJUST;

    cmd->u.timer_adjust.id = id;
    cmd->u.timer_adjust.t  = t;

    mx_push_command(&mx->timer_queue, cmd);
}

/*
 * Remove the timer with id <id>. This timer will not be triggered after all.
 */
void mxRemoveTimer(MX *mx, uint32_t id)
{
    MX_Command *cmd = calloc(1, sizeof(*cmd));

    cmd->cmd_type = MX_CT_TIMER_DELETE;

    cmd->u.timer_delete.id = id;

    mx_push_command(&mx->timer_queue, cmd);
}

/*
 * Return the current UTC timestamp as a double.
 */
double mxNow(void)
{
    return dnow();
}

/*
 * Shut down <mx>. After this function is called, the mxRun function will
 * return.
 */
void mxShutdown(MX *mx)
{
    int fd;

    /* Stop the timer_thread. */

    mx_stop_timer_thread(mx);
    mx_stop_listener_thread(mx);

    /* Stop reader and writer threads for all components. */

    for (fd = 0; fd < paCount(&mx->components); fd++) {
        MX_Component *comp = paGet(&mx->components, fd);

        if (comp == NULL) continue;

        paDrop(&mx->components, fd); /* Remove it from the administration. */

        mx_destroy_component(mx, comp);
    }

    mx->shutting_down = 1;

    close(mx->event_pipe[WR]);
}

/*
 * Destroy <mx>. Call this function only after mxRun has returned.
 */
void mxDestroy(MX *mx)
{
    uint32_t type;

    /* Make sure everything is shut down. */

    mxShutdown(mx);

    /* Destroy all messages. */

    for (type = 0; type < mx->next_message_type; type++) {
        MX_Message *msg = hashGet(&mx->message_by_type, HASH_VALUE(type));

        if (msg == NULL) continue;

        hashDrop(&mx->message_by_type, HASH_VALUE(type));

        if (msg->msg_name != NULL) {
            hashDrop(&mx->message_by_name, HASH_STRING(msg->msg_name));
            free(msg->msg_name);
        }

        mx_destroy_message_subscriptions(msg);

        free(msg);
    }

    close(mx->listen_fd);

    free(mx->mx_name);

    free(mx->me->name);
    free(mx->me->host);

    free(mx->me);

    /* Don't have to free mx->master, because it's either a connected component,
     * in which case it was destroyed using its fd, or I *am* the master, in
     * which case it was destroyed when mx->me was destroyed. */

    free(mx);
}

/*
 * Return a text representation of the errors that have occurred up to now. The
 * returned string becomes the property of the caller, who is responsible for
 * freeing it. Calling this function empties the error string.
 */
char *mxError(void)
{
    return bufDetach(&mx_message);
}
