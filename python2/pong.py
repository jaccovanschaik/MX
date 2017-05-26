#!/usr/bin/env python
# -*- coding: utf-8 -*-

'''
  pong.py: Description
 
  Copyright: (c) 2016 Jacco van Schaik (jacco@jaccovanschaik.net)
  Created:   2016-07-13
  Version:   $Id: pong.py 311 2016-07-15 10:11:40Z jacco $
 
  This software is distributed under the terms of the MIT license. See
  http://www.opensource.org/licenses/mit-license.php for details.
'''

from mx import MX

def on_ping_message(mx, fd, msg_type, msg_version, payload):
  print "Received ping:", mx, fd, msg_type, msg_version, payload

  print "Sending pong"

  mx.send(fd, pong_msg, 0, 'Pong!')

if __name__ == '__main__':
  mx = MX(my_name = 'Pong', mx_host = 'localhost')

  ping_msg = mx.register('Ping')
  pong_msg = mx.register('Pong')

  mx.subscribe(ping_msg, lambda fd, msg_type, msg_version, payload, mx = mx:
      on_ping_message(mx, fd, msg_type, msg_version, payload))

  mx.run()
