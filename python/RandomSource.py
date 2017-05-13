#!/usr/bin/env python
# -*- coding: utf-8 -*-

'''
  RandomSource.py: Description
 
  Copyright: (c) 2016 Jacco van Schaik (jacco@jaccovanschaik.net)
  Created:   2016-07-25
  Version:   $Id: RandomSource.py 335 2016-07-25 10:21:03Z jacco $
 
  This software is distributed under the terms of the MIT license. See
  http://www.opensource.org/licenses/mit-license.php for details.
'''

import struct, random, time, sys

from mx import MX

delta = 0
n_delta = 0

def on_time(mx, ident, t):
  global random_data_msg, delta, n_delta

  delta = (99 * delta + (time.time() - t)) / 100

  n_delta += 1

  print "\r%f" % delta,

  sys.stdout.flush()

  r = random.randint(1, 6)

  payload = struct.pack('>i', r)

  mx.broadcast(random_data_msg, 0, payload)

  mx.createTimer(ident, t + 0.02, lambda ident, t, mx = mx: on_time(mx, ident, t))

mx = MX(my_name = 'Source', mx_host = 'localhost')

random_data_msg = mx.register('RandomData')

mx.createTimer(0, time.time() + 0.1, lambda ident, t, mx = mx: on_time(mx, ident, t))

mx.run()
