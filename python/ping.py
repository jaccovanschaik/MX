#!/usr/bin/env python
# -*- coding: utf-8 -*-

'''
  ping.py: Description
 
  Copyright: (c) 2016 Jacco van Schaik (jacco@jaccovanschaik.net)
  Created:   2016-07-13
  Version:   $Id: ping.py 311 2016-07-15 10:11:40Z jacco $
 
  This software is distributed under the terms of the MIT license. See
  http://www.opensource.org/licenses/mit-license.php for details.
'''

import time

from mx import MX

counter = 0

def on_pong_message(fd, msg_type, msg_version, payload):
  print "Received pong:", fd, msg_type, msg_version, payload

def on_time(mx, t):
  global counter

  print "on_time:", mx, t, counter

  if counter < 10:
    mx.broadcast(ping_msg, 0, 'Ping!')
    counter += 1
    mx.onTime(t + 1, lambda t, mx = mx: on_time(mx, t))
  else:
    mx.shutdown()

if __name__ == '__main__':
  mx = MX('Ping', None, 'localhost')

  ping_msg = mx.register('Ping')
  pong_msg = mx.register('Pong')

  mx.subscribe(pong_msg, on_pong_message)

  mx.onTime(time.time() + 0.1, lambda t, mx = mx: on_time(mx, t))

  mx.run()
