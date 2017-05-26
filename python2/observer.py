#!/usr/bin/env python
# -*- coding: utf-8 -*-

'''
  observer.py: Description
 
  Copyright: (c) 2016 Jacco van Schaik (jacco@jaccovanschaik.net)
  Created:   2016-07-25
  Version:   $Id: observer.py 335 2016-07-25 10:21:03Z jacco $
 
  This software is distributed under the terms of the MIT license. See
  http://www.opensource.org/licenses/mit-license.php for details.
'''

from mx import MX

def on_new_component(fd, msg_name):
  print "on_new_component:", fd, msg_name

def on_end_component(fd, msg_name):
  print "on_end_component:", fd, msg_name

def on_new_subscriber(fd, msg_type):
  print "on_new_subscriber:", fd, msg_type

def on_end_subscriber(fd, msg_type):
  print "on_end_subscriber:", fd, msg_type

def on_new_message(m, msg_type, msg_name):
  print "on_new_message:", m, msg_type, msg_name

  m.onNewSubscriber(msg_type, on_new_subscriber)
  m.onEndSubscriber(msg_type, on_end_subscriber)

if __name__ == '__main__':
  mx = MX(my_name = 'Observer', mx_host = 'localhost')

  mx.onNewComponent(on_new_component)
  mx.onEndComponent(on_end_component)

  mx.onNewMessage(lambda msg_type, msg_name, m = mx: on_new_message(m, msg_type, msg_name))

  mx.run()
