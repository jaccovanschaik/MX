/* pymx.c: Description
 *
 * Copyright: (c) 2016-2025 Jacco van Schaik (jacco@jaccovanschaik.net)
 * Created:   2016-07-06
 * Version:   $Id: pymx.c 461 2022-01-31 09:02:30Z jacco $
 *
 * This software is distributed under the terms of the MIT license. See
 * http://www.opensource.org/licenses/mit-license.php for details.
 */

#include "../src/libmx.h"

#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include <structmember.h>

typedef struct {
    PyObject_HEAD
    MX *mx;
} MXObject;

static void MX_Dealloc(MXObject *self)
{
    if (self->mx != NULL) {
        mxShutdown(self->mx);
        mxDestroy(self->mx);

        self->mx = NULL;
    }

    Py_TYPE(self)->tp_free((PyObject*) self);
}

static int MX_Init(MXObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"my_name", "mx_name", "mx_host", NULL};
    char *my_name = NULL, *mx_name = NULL, *mx_host = NULL;

    if (self->mx != NULL) {
        mxShutdown(self->mx);
        mxDestroy(self->mx);

        self->mx = NULL;
    }

    if (PyArg_ParseTupleAndKeywords(args, kwds, "|zzz:init",
                kwlist, &my_name, &mx_name, &mx_host) == 0) {
        return -1;
    }

    if (mx_host == NULL) {
        self->mx = mxMaster(mx_name, my_name, false);
    }
    else if (my_name != NULL) {
        self->mx = mxClient(mx_host, mx_name, my_name);
    }
    else {
        PyErr_SetString(PyExc_AttributeError, "missing parameter \"my_name\".");
        return -1;
    }

    if (self->mx == NULL) {
        char *error = mxError();
        PyErr_SetString(PyExc_IOError, error);
        free(error);
        return -1;
    }

    return 0;
}

/* const char *mxEffectiveName(const char *mx_name); */
static PyObject *MX_EffectiveName(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    PyObject *result;

    static char *kwlist[] = {"mx_name", NULL};
    char *mx_name = NULL;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "|z:effectiveName",
                kwlist, &mx_name) == 0) {
        return NULL;
    }

    result = PyUnicode_FromString(mxEffectiveName(mx_name));

    return result;
}

/* const char *mxEffectiveHost(const char *mx_host); */
static PyObject *MX_EffectiveHost(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    PyObject *result;

    static char *kwlist[] = {"mx_host", NULL};
    char *mx_host = NULL;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "|z:effectiveHost",
                kwlist, &mx_host) == 0) {
        return NULL;
    }

    result = PyUnicode_FromString(mxEffectiveHost(mx_host));

    return result;
}

/* uint16_t mxEffectivePort(const char *mx_name); */
static PyObject *MX_EffectivePort(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    PyObject *result;

    static char *kwlist[] = {"mx_name", NULL};
    char *mx_name = NULL;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "|z:effectivePort",
                kwlist, &mx_name) == 0) {
        return NULL;
    }

    result = PyLong_FromLong(mxEffectivePort(mx_name));

    return result;
}

/* const char *mxMyName(const MX *mx); */
static PyObject *MX_MyName(MXObject *self)
{
    PyObject *result;

    result = PyUnicode_FromString(mxMyName(self->mx));

    return result;
}

/* const char *mxName(const MX *mx); */
static PyObject *MX_Name(MXObject *self)
{
    PyObject *result;

    result = PyUnicode_FromString(mxName(self->mx));

    return result;
}

/* const char *mxHost(const MX *mx); */
static PyObject *MX_Host(MXObject *self)
{
    PyObject *result;

    result = PyUnicode_FromString(mxHost(self->mx));

    return result;
}

/* uint16_t mxPort(const MX *mx); */
static PyObject *MX_Port(MXObject *self)
{
    PyObject *result;

    result = PyLong_FromLong(mxPort(self->mx));

    return result;
}

/* uint32_t mxRegister(MX *mx, const char *msg_name); */
static PyObject *MX_Register(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    PyObject *result;

    static char *kwlist[] = {"msg_name", NULL};
    char *msg_name = NULL;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "|z:register",
                kwlist, &msg_name) == 0) {
        return NULL;
    }

    result = PyLong_FromLong(mxRegister(self->mx, msg_name));

    return result;
}

/* const char *mxMessageName(MX *mx, uint32_t type); */
static PyObject *MX_MessageName(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    PyObject *result;

    static char *kwlist[] = {"msg_type", NULL};
    uint32_t msg_type = 0;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "I:messageName",
                kwlist, &msg_type) == 0) {
        return NULL;
    }

    result = PyUnicode_FromString(mxMessageName(self->mx, msg_type));

    return result;
}

/* const char *mxComponentName(MX *mx, int fd); */
static PyObject *MX_ComponentName(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    PyObject *result;

    static char *kwlist[] = {"fd", NULL};
    int fd = -1;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "i:componentName",
                kwlist, &fd) == 0) {
        return NULL;
    }

    result = PyUnicode_FromString(mxComponentName(self->mx, fd));

    return result;
}

void subscribe_cb(MX *mx, int fd, uint32_t type, uint32_t version,
            char *payload, uint32_t size, void *udata)
{
    PyObject *r;
    PyObject *handler = udata;
    Py_ssize_t py_size = size;
    PyObject *arglist = Py_BuildValue("(iIIy#)", fd, type, version, payload, py_size);

    free(payload);

    r = PyObject_CallObject(handler, arglist);

    if (r == NULL) {
        printf("PyObject_CallObject returned NULL\n");
        PyErr_Print();
    }
    else {
        Py_DECREF(r);
    }

    Py_DECREF(arglist);
}

/* void mxSubscribe(MX *mx, uint32_t type, handler, udata);
 *      void (*handler)(MX *mx, int fd, uint32_t type, uint32_t version,
 *          char *payload, uint32_t size, void *udata),
 */
static PyObject *MX_Subscribe(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"msg_type", "handler", NULL};
    uint32_t msg_type = 0;
    PyObject *handler;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "IO:subscribe",
                kwlist, &msg_type, &handler) == 0) {
        return NULL;
    }
    else if (!PyCallable_Check(handler)) {
        PyErr_SetString(PyExc_TypeError, "\"handler\" must be callable");
        return NULL;
    }

    Py_INCREF(handler);         /* Add a reference to new callback */

    return PyLong_FromLong(mxSubscribe(self->mx, msg_type, subscribe_cb, handler));
}

/* void mxCancel(MX *mx, uint32_t type); */
static PyObject *MX_Cancel(MXObject *self, PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"msg_type", NULL};
    uint32_t msg_type = 0;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "I:cancel",
                kwlist, &msg_type) == 0) {
        return NULL;
    }

    return PyLong_FromLong(mxCancel(self->mx, msg_type));
}

static void on_subscriber_cb(MX *mx, int fd, uint32_t msg_type, void *udata)
{
    PyObject *r;
    PyObject *handler = udata;
    PyObject *arglist = Py_BuildValue("(iI)", fd, msg_type);

    r = PyObject_CallObject(handler, arglist);

    if (r == NULL) {
        PyErr_Print();
    }
    else {
        Py_DECREF(r);
    }

    Py_DECREF(arglist);
}

/* void mxOnNewSubscriber(MX *mx, uint32_t type,
 *      void (*handler)(MX *mx, uint32_t type, int fd, void *udata),
 *      void *udata);
 */
static PyObject *MX_OnNewSubscriber(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"msg_type", "handler", NULL};
    uint32_t msg_type = 0;
    PyObject *handler;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "IO:onNewSubscriber",
                kwlist, &msg_type, &handler) == 0) {
        return NULL;
    }
    else if (!PyCallable_Check(handler)) {
        PyErr_SetString(PyExc_TypeError, "\"handler\" must be callable");
        return NULL;
    }

    Py_INCREF(handler);         /* Add a reference to new callback */

    mxOnNewSubscriber(self->mx, msg_type, on_subscriber_cb, handler);

    Py_RETURN_NONE;
}

/* mxOnEndSubscriber(MX *mx, uint32_t type,
 *      void (*handler)(MX *mx, uint32_t type, int fd, void *udata),
 *      void *udata);
 */
static PyObject *MX_OnEndSubscriber(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"msg_type", "handler", NULL};
    uint32_t msg_type = 0;
    PyObject *handler;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "IO:onNewSubscriber",
                kwlist, &msg_type, &handler) == 0) {
        return NULL;
    }
    else if (!PyCallable_Check(handler)) {
        PyErr_SetString(PyExc_TypeError, "\"handler\" must be callable");
        return NULL;
    }

    Py_INCREF(handler);         /* Add a reference to new callback */

    mxOnEndSubscriber(self->mx, msg_type, on_subscriber_cb, handler);

    Py_RETURN_NONE;
}

static void on_component_cb(MX *mx, int fd, const char *name, void *udata)
{
    PyObject *r;
    PyObject *handler = udata;
    PyObject *arglist = Py_BuildValue("(is)", fd, name);

    r = PyObject_CallObject(handler, arglist);

    if (r == NULL) {
        PyErr_Print();
    }
    else {
        Py_DECREF(r);
    }

    Py_DECREF(arglist);
}

/* void mxOnNewComponent(MX *mx,
 *      void (*handler)(MX *mx, int fd, const char *name, void *udata),
 *      void *udata);
 */
static PyObject *MX_OnNewComponent(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"handler", NULL};
    PyObject *handler;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "O:onNewComponent",
                kwlist, &handler) == 0) {
        return NULL;
    }
    else if (!PyCallable_Check(handler)) {
        PyErr_SetString(PyExc_TypeError, "\"handler\" must be callable");
        return NULL;
    }

    Py_INCREF(handler);         /* Add a reference to new callback */

    mxOnNewComponent(self->mx, on_component_cb, handler);

    Py_RETURN_NONE;
}

/* void mxOnEndComponent(MX *mx,
 *      void (*handler)(MX *mx, int fd, const char *name, void *udata),
 *      void *udata);
 */
static PyObject *MX_OnEndComponent(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"handler", NULL};
    PyObject *handler;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "O:onEndComponent",
                kwlist, &handler) == 0) {
        return NULL;
    }
    else if (!PyCallable_Check(handler)) {
        PyErr_SetString(PyExc_TypeError, "\"handler\" must be callable");
        return NULL;
    }

    Py_INCREF(handler);         /* Add a reference to new callback */

    mxOnEndComponent(self->mx, on_component_cb, handler);

    Py_RETURN_NONE;
}

static void on_new_message_cb(MX *mx,
        uint32_t type, const char *name, void *udata)
{
    PyObject *r;
    PyObject *handler = udata;
    PyObject *arglist = Py_BuildValue("(Is)", type, name);

    r = PyObject_CallObject(handler, arglist);

    if (r == NULL) {
        PyErr_Print();
    }
    else {
        Py_DECREF(r);
    }

    Py_DECREF(arglist);
}

/* void mxOnNewMessage(MX *mx,
 *      void (*handler)(MX *mx, uint32_t type, const char *name, void *udata),
 *      void *udata);
 */
static PyObject *MX_OnNewMessage(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"handler", NULL};
    PyObject *handler;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "O:onNewMessage",
                kwlist, &handler) == 0) {
        return NULL;
    }
    else if (!PyCallable_Check(handler)) {
        PyErr_SetString(PyExc_TypeError, "\"handler\" must be callable");
        return NULL;
    }

    Py_INCREF(handler);         /* Add a reference to new callback */

    mxOnNewMessage(self->mx, on_new_message_cb, handler);

    Py_RETURN_NONE;
}

/* void mxSend(MX *mx, int fd, uint32_t type, uint32_t version,
 *      const void *payload, uint32_t size); */
static PyObject *MX_Send(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"fd", "msg_type", "msg_version", "payload", NULL};
    int fd;
    uint32_t msg_type, msg_version;
    char *payload;
    Py_ssize_t size;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "iIIy#:send",
                kwlist, &fd, &msg_type, &msg_version, &payload, &size) == 0) {
        return NULL;
    }

    mxSend(self->mx, fd, msg_type, msg_version, payload, size);

    Py_RETURN_NONE;
}

/* void mxBroadcast(MX *mx,
 *      uint32_t type, uint32_t version, const void *payload, uint32_t size); */
static PyObject *MX_Broadcast(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"msg_type", "msg_version", "payload", NULL};
    uint32_t msg_type, msg_version;
    char *payload;
    Py_ssize_t size;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "IIy#:broadcast",
                kwlist, &msg_type, &msg_version, &payload, &size) == 0) {
        return NULL;
    }

    mxBroadcast(self->mx, msg_type, msg_version, payload, size);

    Py_RETURN_NONE;
}

/* int mxAwait(MX *mx, int fd, double timeout,
 *      uint32_t type, uint32_t *version, char **payload, uint32_t *size);
 */
static PyObject *MX_Await(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    PyObject *result;

    static char *kwlist[] = {"fd", "timeout", "type", NULL};
    int r, fd;
    double timeout;
    uint32_t type, version = 0;
    char *payload = NULL;
    uint32_t size = 0;
    Py_ssize_t py_size;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "idI:await",
                kwlist, &fd, &timeout, &type) == 0) {
        return NULL;
    }

    r = mxAwait(self->mx, fd, timeout, type, &version, &payload, &size);

    py_size = size;

    result = Py_BuildValue("(iIy#)", r, version, payload, py_size);

    return result;
}

/* int mxSendAndWait(MX *mx, int fd, double timeout,
 *      uint32_t reply_type, uint32_t *reply_version,
 *      char **reply_payload, uint32_t *reply_size,
 *      uint32_t request_type, uint32_t request_version,
 *      const char *request_payload, uint32_t request_size);
 */
static PyObject *MX_SendAndWait(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    PyObject *result;

    static char *kwlist[] = {
        "fd", "timeout", "reply_type", "request_type",
        "request_version", "request_payload", NULL
    };

    int r, fd;
    double timeout = 0;
    uint32_t reply_type = 0, request_type = 0;
    uint32_t reply_version = 0, request_version = 0;
    char *request_payload = NULL, *reply_payload = NULL;
    Py_ssize_t request_size = 0;
    uint32_t reply_size = 0;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "idIIIy#:sendAndAwait",
                kwlist, &fd, &timeout, &reply_type, &request_type,
                &request_version, &request_payload, &request_size) == 0) {
        return NULL;
    }

    r = mxSendAndWait(self->mx, fd, timeout,
            reply_type, &reply_version, &reply_payload, &reply_size,
            request_type, request_version, request_payload, request_size);

    Py_ssize_t py_reply_size = reply_size;

    result = Py_BuildValue("(iIy#)", r, reply_version, reply_payload, py_reply_size);

    return result;
}

void timer_callback(MX *mx, MX_Timer *timer, double t, void *udata)
{
    PyObject *r;
    PyObject *handler = udata;
    PyObject *arglist = Py_BuildValue("(Kd)", (uint64_t) timer, t);

    r = PyObject_CallObject(handler, arglist);

    if (r == NULL) {
        PyErr_Print();
    }
    else {
        Py_DECREF(r);
    }

    Py_DECREF(arglist);
}

/* void mxCreateTimer(MX *mx, unit32_t id, double t,
 *      void (*handler)(MX *mx, double t, void *udata),
 *      void *udata);
 */
static PyObject *MX_CreateTimer(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"t", "handler", NULL};
    double time;
    PyObject *handler;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "dO:createTimer",
                kwlist, &time, &handler) == 0) {
        return NULL;
    }
    else if (!PyCallable_Check(handler)) {
        PyErr_SetString(PyExc_TypeError, "\"handler\" must be callable");
        return NULL;
    }

    Py_INCREF(handler);         /* Add a reference to new callback */

    MX_Timer *timer = mxCreateTimer(self->mx, time, timer_callback, handler);

    PyObject *result = PyLong_FromVoidPtr(timer);

    return result;
}

/* void mxAdjustTimer(MX *mx, MX_Timer *timer, double t); */
static PyObject *MX_AdjustTimer(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"timer", "t", NULL};
    MX_Timer *timer;
    double time;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "Kd:adjustTimer",
                kwlist, &timer, &time) == 0) {
        return NULL;
    }

    mxAdjustTimer(self->mx, timer, time);

    Py_RETURN_NONE;
}

/* void mxRemoveTimer(MX *mx, MX_Timer *timer); */
static PyObject *MX_RemoveTimer(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    static char *kwlist[] = {"timer", NULL};
    MX_Timer *timer;

    if (PyArg_ParseTupleAndKeywords(args, kwds, "K:removeTimer",
                kwlist, &timer) == 0) {
        return NULL;
    }

    mxRemoveTimer(self->mx, timer);

    Py_RETURN_NONE;
}

/* int mxConnectionNumber(MX *mx); */
static PyObject *MX_ConnectionNumber(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    PyObject *result;

    result = PyLong_FromLong(mxConnectionNumber(self->mx));

    return result;
}

/* int mxProcessEvents(MX *mx); */
static PyObject *MX_ProcessEvents(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    PyObject *result;

    result = PyLong_FromLong(mxProcessEvents(self->mx));

    return result;
}

/* int mxRun(MX *mx); */
static PyObject *MX_Run(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    PyObject *result;

    int r = mxRun(self->mx);

    result = PyLong_FromLong(r);

    return result;
}

/* void mxShutdown(MX *mx); */
static PyObject *MX_Shutdown(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    mxShutdown(self->mx);

    Py_RETURN_NONE;
}

/* char *mxError(void); */
static PyObject *MX_Error(MXObject *self,
        PyObject *args, PyObject *kwds)
{
    PyObject *result;
    char *error_msg = mxError();

    result = PyUnicode_FromString(error_msg);

    free(error_msg);

    return result;
}

static PyMethodDef MX_methods[] = {
    { "effectiveName", (PyCFunction) MX_EffectiveName,
      METH_KEYWORDS | METH_VARARGS | METH_STATIC,
      "mx.effectiveName(mx_name = None)\n\n"
      "Return the mx_name to use if <mx_name> was given to mxClient(). If it\n"
      "is a valid name (i.e. not None) use it. Otherwise use the environment\n"
      "variable MX_NAME if it is set. Otherwise use the environment variable\n"
      "USER (the user's login name) if it is set. Otherwise give up and\n"
      "return None."
    },
    { "effectiveHost", (PyCFunction) MX_EffectiveHost,
      METH_KEYWORDS | METH_VARARGS | METH_STATIC,
      "mx.effectiveHost(mx_host = None)\n\n"
      "Return the mx_host to use if <mx_host> was given to mxClient() or\n"
      "mxMaster(). If it is a valid name (i.e. not NULL) use it. Otherwise\n"
      "use the environment variable MX_HOST if it is set. Otherwise simply\n"
      "use \"localhost\"."
    },
    { "effectivePort", (PyCFunction) MX_EffectivePort,
      METH_KEYWORDS | METH_VARARGS | METH_STATIC,
      "mx.effectivePort(mx_name = None)\n\n"
      "Return the listen port that the master component would use for\n"
      "mx_name <mx_name>. The mx.effectiveName() function will be\n"
      "used to determine the effective MX name."
    },
    { "myName", (PyCFunction) MX_MyName, METH_NOARGS,
      "mx.mxName(self)\n\n"
      "Returns the name of the local component."
    },
    { "name", (PyCFunction) MX_Name, METH_NOARGS,
      "mx.name(self)\n\n"
      "Returns the MX name used by <self>."
    },
    { "host", (PyCFunction) MX_Host, METH_NOARGS,
      "mx.host(self)\n\n"
      "Returns the current MX host (the host on which the master runs)."
    },
    { "port", (PyCFunction) MX_Port, METH_NOARGS,
      "mx.port(self)\n\n"
      "Returns the current MX port (the port on which the master listens)."
    },
    { "register", (PyCFunction) MX_Register,
       METH_KEYWORDS | METH_VARARGS,
      "mx.register(self, msg_name)\n\n"
      "Register the message name <msg_name>. Returns the associated message\n"
      "type id."
    },
    { "messageName", (PyCFunction) MX_MessageName, METH_KEYWORDS | METH_VARARGS,
      "mx.messageName(self, msg_type)\n\n"
      "Returns the message name associated with message type <msg_type>.",
    },
    { "componentName", (PyCFunction) MX_ComponentName,
      METH_KEYWORDS | METH_VARARGS,
      "mx.componentName(self, fd)\n\n"
      "Returns the name of the component connected to us on fd <fd>.",
    },
    { "subscribe", (PyCFunction) MX_Subscribe,
      METH_KEYWORDS | METH_VARARGS,
      "mx.subscribe(self, msg_type, handler)\n\n"
      "Subscribe to messages of type <msg_type> and call <handler> for each "
      "received message. Returns <0 or >0 for errors and notices (in which "
      "case you should check mx.error) or 0. <handler> should have the "
      "following signature:\n\n"
      "\thandler(fd, msg_type, msg_version, payload).\n\n"
      "where <msg_type> is the type of the received message, <msg_version> is "
      "its version and <payload> is its payload."
    },
    { "cancel", (PyCFunction) MX_Cancel,
      METH_KEYWORDS | METH_VARARGS,
      "mx.cancel(self, msg_type)\n\n"
      "Cancel our subscription to messages of type <msg_type>. Returns <0 or "
      ">0 for errors and notices (in which case you should check mx.error) or "
      "0."
    },
    { "onNewSubscriber", (PyCFunction) MX_OnNewSubscriber,
      METH_KEYWORDS | METH_VARARGS,
      "mx.onNewSubscriber(self, msg_type, handler)\n\n"
      "Call <handler> whenever a component subscribes to messages of type "
      "<msg_type>. <handler> should have the following signature:\n\n"
      "\thandler(fd, msg_type)\n\n"
      "where <fd> is the file descriptor of our connection with the component, "
      "and <msg_type> is the message type that they've subscribed to."
    },
    { "onEndSubscriber", (PyCFunction) MX_OnEndSubscriber,
      METH_KEYWORDS | METH_VARARGS,
      "mx.onEndSubscriber(self, msg_type, handler)\n\n"
      "Call <handler> whenever a component cancels its subscription to "
      "messages of type <msg_type>. <handler> should have the following "
      "signature:\n\n"
      "\thandler(fd, msg_type)\n\n"
      "where <fd> is the file descriptor of our connection with the component, "
      "and <msg_type> is the message type that they've subscribed to."
    },
    { "onNewComponent", (PyCFunction) MX_OnNewComponent,
      METH_KEYWORDS | METH_VARARGS,
      "mx.onNewComponent(self, handler)\n\n"
      "Call <handler> when a new component reports in. <handler> should have "
      "the following signature:\n\n"
      "\thandler(fd, name)\n\n"
      "where <fd> is the file descriptor of our connection with the new "
      "component, and <name> is its name."
    },
    { "onEndComponent", (PyCFunction) MX_OnEndComponent,
      METH_KEYWORDS | METH_VARARGS,
      "mx.onEndComponent(self, handler)\n\n"
      "Call <handler> when the connection with a component is lost. <handler> "
      "should have the following signature:\n\n"
      "\thandler(fd, name)\n\n"
      "where <fd> was the file descriptor of our connection with the "
      "component, and <name> was its name."
    },
    { "onNewMessage", (PyCFunction) MX_OnNewMessage,
      METH_KEYWORDS | METH_VARARGS,
      "mx.onNewMessage(self, handler)\n\n"
      "Call <handler> when a new message type is registered. <handler> "
      "should have the following signature:\n\n"
      "\thandler(msg_type, msg_name)\n\n"
      "where <msg_type> is the type and <msg_name> is the name of the new "
      "message."
    },
    { "send", (PyCFunction) MX_Send,
      METH_KEYWORDS | METH_VARARGS,
      "mx.send(self, fd, msg_type, msg_version, payload)\n\n"
      "Send a message of type <msg_type> on file descriptor <fd>. The "
      "message is sent with version <msg_version> and payload <payload>."
    },
    { "broadcast", (PyCFunction) MX_Broadcast,
      METH_KEYWORDS | METH_VARARGS,
      "mx.broadcast(self, msg_type, msg_version, payload)\n\n"
      "Broadcast a message with type <msg_type>, version <msg_version> and "
      "payload <payload> to all subscribers of this message type."
    },
    { "await", (PyCFunction) MX_Await,
      METH_KEYWORDS | METH_VARARGS,
      "mx.await(self, fd, timeout, msg_type) -> "
      "(r, reply_version, reply_payload)\n\n"
      "Wait for a message of type <type> to arrive on file descriptor <fd>. "
      "Returns a tuple consisting of a return value, and the version and "
      "payload of the received message. If the message does not arrive within "
      "<timeout> seconds, the return value will be 0, the version will be 0 "
      "and the payload will be None. Otherwise the return value will be 1 and "
      "the version and payload of the received message will be returned."
    },
    { "sendAndWait", (PyCFunction) MX_SendAndWait,
      METH_KEYWORDS | METH_VARARGS,
      "mx.sendAndWait(self, fd, timeout, reply_type, "
      "request_type, request_version, request_payload) -> "
      "(r, reply_version, reply_payload)\n\n"
      "Send a message with type <request_type>, version <request_version> "
      "and payload <request_payload>, then wait for a reply with type "
      "<reply_type>. Returns a tuple consisting of a return value, and the "
      "version and payload of the received reply. If the reply does not arrive "
      "within <timeout> seconds, the return value will be 0, the version will "
      "be 0 and the payload will be None. Otherwise the return value will be 1 "
      "and the version and payload of the received reply will be returned."
    },
    { "createTimer", (PyCFunction) MX_CreateTimer,
      METH_KEYWORDS | METH_VARARGS,
      "mx.createTimer(self, time, handler)\n\n"
      "Create a timer to call <handler> at time <time>. <time> is the number "
      "of seconds since 00:00:00 01-01-1970 UTC as a floating point number, "
      "as returned by time.time(). Returns a timer id that can be used in "
      "adjustTimer and removeTimer below."
    },
    { "adjustTimer", (PyCFunction) MX_AdjustTimer,
      METH_KEYWORDS | METH_VARARGS,
      "mx.adjustTimer(self, id, time)\n\n"
      "Adjust the timeout of the timer with id <id> to time <time>. <time> "
      "is the number of seconds since 00:00:00 01-01-1970 UTC as a floating "
      "point number, as returned by time.time()."
    },
    { "removeTimer", (PyCFunction) MX_RemoveTimer,
      METH_KEYWORDS | METH_VARARGS,
      "mx.removeTimer(self, id)\n\n"
      "Remove the timer with id <id>."
    },
    { "connectionNumber", (PyCFunction) MX_ConnectionNumber,
      METH_NOARGS,
      "mx.connectionNumber(self) -> fd\n\n"
      "Return the file descriptor on which all events associated with <mx> "
      "arrive."
    },
    { "processEvents", (PyCFunction) MX_ProcessEvents,
      METH_NOARGS,
      "mx.processEvents(self) -> r\n\n"
      "Process any pending events associated with <mx>. Returns -1 if an error "
      "occurred, 1 if event processing has finished normally and 0 if no more "
      "events are forthcoming (so there's no sense in waiting for them "
      "anymore)."
    },
    { "run", (PyCFunction) MX_Run,
      METH_NOARGS,
      "mx.run(self) -> r\n\n"
      "Enter a loop listening for and handling events. Returns -1 if an error "
      "occurred or 0 if mx.shutdown() was called."
    },
    { "shutdown", (PyCFunction) MX_Shutdown,
      METH_NOARGS,
      "mx.shutdown(self)\n\n"
      "Shut down <mx>. After this function is called, the mxRun function will "
      "return.",
    },
    { "error", (PyCFunction) MX_Error,
      METH_NOARGS | METH_STATIC,
      "mx.error()\n\n"
      "Return a text representation of the errors that have occurred up to "
      "now. Calling this function clear the error string."
    },
    {NULL}  /* Sentinel */
};

static PyTypeObject MXType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "mx.MX",                    /*tp_name*/
    sizeof(MXObject),           /*tp_basicsize*/
    0,                          /*tp_itemsize*/
    (destructor)MX_Dealloc,     /*tp_dealloc*/
    0,                          /*tp_print*/
    0,                          /*tp_getattr*/
    0,                          /*tp_setattr*/
    0,                          /*tp_compare*/
    0,                          /*tp_repr*/
    0,                          /*tp_as_number*/
    0,                          /*tp_as_sequence*/
    0,                          /*tp_as_mapping*/
    0,                          /*tp_hash */
    0,                          /*tp_call*/
    0,                          /*tp_str*/
    0,                          /*tp_getattro*/
    0,                          /*tp_setattro*/
    0,                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT, /* | Py_TPFLAGS_BASETYPE, */ /*tp_flags*/
    "The MX object.\n\n"
    "Constructor arguments:\n"
    "my_name - The name of the component. If this is None and the component is\n"
    "          a master (see below) the name \"master\" will be used. Otherwise\n"
    "          it is an error for this to be None.\n"
    "mx_name - The name of the MX to create or connect to. If this is None\n"
    "          and the environment variable \"MX_NAME\" is set, its value will\n"
    "          be used. Otherwise the current username will be used.\n"
    "mx_host - The host of the mx to connect to. If this is None the\n"
    "          component will act as a master component. Otherwise the component\n"
    "          will try to connect to a master component running on the given host.\n",
    0,		                /* tp_traverse */
    0,		                /* tp_clear */
    0,		                /* tp_richcompare */
    0,		                /* tp_weaklistoffset */
    0,		                /* tp_iter */
    0,		                /* tp_iternext */
    MX_methods,                 /* tp_methods */
    0,                          /* tp_members */
    0,                          /* tp_getset */
    0,                          /* tp_base */
    0,                          /* tp_dict */
    0,                          /* tp_descr_get */
    0,                          /* tp_descr_set */
    0,                          /* tp_dictoffset */
    (initproc)MX_Init,          /* tp_init */
    0,                          /* tp_alloc */
    0,                          /* tp_new */
};

static struct PyModuleDef mx_module = {
    PyModuleDef_HEAD_INIT,          /* PyModuleDef_Base m_base */
    "mx",                           /* const char* m_name */
    "MX Module",                    /* const char* m_doc */
    -1,                             /* Py_ssize_t m_size */
    NULL,                           /* PyMethodDef *m_methods */
    NULL,                           /* struct PyModuleDef_Slot *m_slots */
    NULL,                           /* traverseproc m_traverse */
    NULL,                           /* inquiry m_clear */
    NULL                            /* freefunc m_free */
};

#ifndef PyMODINIT_FUNC	/* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif

PyMODINIT_FUNC PyInit_mx(void)
{
    PyObject *m;

    MXType.tp_new = PyType_GenericNew;

    if (PyType_Ready(&MXType) < 0)
        return NULL;

    m = PyModule_Create(&mx_module);

    if (m == NULL)
      return NULL;

    Py_INCREF(&MXType);

    PyModule_AddObject(m, "MX", (PyObject *)&MXType);

    return m;
}
