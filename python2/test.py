#!/usr/bin/env python
# -*- coding: utf-8 -*-

'''
  test.py: Description
 
  Copyright: (c) 2016 Jacco van Schaik (jacco@jaccovanschaik.net)
  Created:   2016-07-01
  Version:   $Id: test.py 321 2016-07-18 19:55:05Z jacco $
 
  This software is distributed under the terms of the MIT license. See
  http://www.opensource.org/licenses/mit-license.php for details.
'''

import time

from mx import MX

print "EffectiveName without input:", MX.effectiveName()
print "EffectivePort without input:", MX.effectivePort()
print "EffectiveHost without input:", MX.effectiveHost()

mx_name = 'Jansen'

print "EffectivePort with input '%s': %d" % (mx_name, MX.effectivePort(mx_name))
print "EffectiveName with input '%s': %s" % (mx_name, MX.effectiveName(mx_name))

mx_host = 'www.jaccovanschaik.net'

print "EffectiveHost with input '%s': %s" % (mx_host, MX.effectiveHost(mx_host))

my_name = "theMaster"

print "Creating master with name '%s'" % my_name

mx = MX(my_name)

print "MX:", mx

print "connectionNumber:", mx.connectionNumber()

print "mx.myName:", mx.myName()
print "mx.name:", mx.name()
print "mx.port:", mx.port()
print "mx.host:", mx.host()

test_message_1 = mx.register('test_message_1')
test_message_2 = mx.register('test_message_2')

print test_message_1
print test_message_2

print mx.messageName(test_message_1)
print mx.messageName(test_message_2)

def on_time_cb(t, mx):
  print "t: ", t
  print "mx:", mx

  mx.shutdown()

mx.onTime(time.time() + 1, lambda t, mx = mx: on_time_cb(t, mx))

def subscribe_cb(msg_type, msg_version, payload):
  print "msg_type:", msg_type
  print "msg_version:", msg_version
  print "payload:", payload

mx.subscribe(test_message_1, subscribe_cb)
mx.cancel(test_message_1);

def on_new_subscriber(fd, msg_type):
  print "Component on fd", fd, "subscribed to msg_type", msg_type

mx.onNewSubscriber(test_message_2, on_new_subscriber)

def on_end_subscriber(fd, msg_type):
  print "Component on fd", fd, "unsubscribed from msg_type", msg_type

mx.onEndSubscriber(test_message_2, on_end_subscriber)

def on_new_component(fd, name):
  print "New component", name, "on fd", fd

mx.onNewComponent(on_new_component)

def on_end_component(fd, name):
  print "Lost component", name, "on fd", fd

mx.onEndComponent(on_end_component)

def on_new_message(msg_type, msg_name):
  print "New message type with name", msg_name, "and type", msg_type

mx.onNewMessage(on_new_message)

fd = 5
timeout = 5

r, msg_version, payload = mx.await(fd, timeout, test_message_1)

request_type = test_message_1
request_version = 0
request_payload = ''
reply_type = test_message_2

r, reply_version, reply_payload = mx.sendAndWait(fd, timeout, reply_type,
    request_type, request_version, request_payload)

print "await(%d, %f, %u) returned (%d, %u, %r)" \
    % (fd, timeout, test_message_1, r, msg_version, payload)

r = mx.run()

print "mx.run() returned", r

print "error: %r" % mx.error()
