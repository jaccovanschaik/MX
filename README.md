mx
==

Introduction
------------

MX is a light-weight communication library written in C. It provides
programs that use it with a way to publish and subscribe to messages.
There is no predetermined structure to the payload of a message. To MX,
a message payload is a simple, opaque byte string. In addition to a C
interface there is also a Python binding.

Design considerations
---------------------

MX is not designed to be in operation 24/7. Although it is possible,
with some care, to build such a system using MX, that is not what MX was
designed for. It is assumed that systems built on top of MX will be
started, run for some time and then be shut down.

(The reason for this is that message types, once
created, are never re-used, which makes them a limited resource. If you
design a system that only allocates a fixed number of message types it
should be possible to run it indefinitely, but MX will not prevent you
from exhausting the available message types. On the other hand, a
message type is a 32-bit unsigned integer so it'll take a while to
exhaust that supply).

MX is not designed to be particularly resilient against external
hardware or software failures. It is assumed that a run-time failure in
any of the participating components signifies a serious problem that
will be rectified before trying again. MX will report the failure but
will not try to restart the failed component, for example.

Also, the master component constitutes a single point of failure. If it
crashes or is killed all other participating components will also exit.
This should not be a big problem for a standalone master (started using
mx master) because it is fairly robust, but if another component assumes
the master role (see the mxMaster function) and it
quits or crashes, all other participating components will exit too.

Finally, security was not a design consideration for MX. If you can
connect to the master component it is trivially easy to kill it with a
hand-crafted message (indeed, this message was specifically designed to
make it so), components connect to each other without any
authentication, and messages are exchanged without any encryption. MX is
expected to run on a private network where all connected devices can be
trusted.

TL;DR: If you are planning to use MX for a nuclear power station you
should probably reconsider.
