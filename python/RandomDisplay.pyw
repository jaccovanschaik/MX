#!/usr/bin/env python
# -*- coding: utf-8 -*-

'''
  RandomDisplay.pyw: Description
 
  Copyright: (c) 2016 Jacco van Schaik (jacco@jaccovanschaik.net)
  Created:   2016-07-25
  Version:   $Id: RandomDisplay.pyw 335 2016-07-25 10:21:03Z jacco $
 
  This software is distributed under the terms of the MIT license. See
  http://www.opensource.org/licenses/mit-license.php for details.
'''

import sys, struct

from mx import MX

from PyQt4.QtGui import QApplication
from PyQt4.QtCore import QSocketNotifier

import PyQt4.uic as uic

class RandomDisplay(QApplication):
  def __init__(self, args):
    QApplication.__init__(self, args)

    self._ui = uic.loadUi("RandomDisplay.ui")

    self._mx = MX(my_name = 'Display', mx_host = 'localhost')

    self._random_data_msg = self._mx.register('RandomData')

    self._mx.subscribe(self._random_data_msg, self._handle_random_data_msg)

    self._notifier = QSocketNotifier(self._mx.connectionNumber(), QSocketNotifier.Read, self)
    self._notifier.activated.connect(self._handle_mx_events)

    self._ui.show()

  def _handle_mx_events(self):
    r = self._mx.processEvents()

    if r > 0:     # Everything in order, wait for the next event(s)
      pass
    elif r == 0:  # No more events expected, just quit.
      self.exit()
    elif r < 0:   # Error, print message and quit.
      print mx.error()
      self.exit()

  def _handle_random_data_msg(self, fd, msg_type, msg_version, payload):
    data, = struct.unpack('>i', payload)

    self._ui.label.setText(str(data))

if __name__ == '__main__':
  app = RandomDisplay(sys.argv)

  app.exec_()
