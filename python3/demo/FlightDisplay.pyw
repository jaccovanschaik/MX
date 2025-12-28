#!/usr/bin/env python3
# -*- coding: utf-8 -*-

'''
  FlightDisplay.pyw: Display flights, possibly at a specific gate.

  If the --gate/-g option is given, followed by a gate name, only flights arriving or departing at
  that gate will be shown. Otherwise all flights are shown.

  Copyright: (c) 2016-2025 Jacco van Schaik (jacco@jaccovanschaik.net)
  Created:   2016-08-03
  Version:   $Id: FlightDisplay.pyw 456 2020-10-22 10:35:42Z jacco $

  This software is distributed under the terms of the MIT license. See
  http://www.opensource.org/licenses/mit-license.php for details.
'''

import sys, time, argparse, datetime

from mx import MX

from PyQt5.QtWidgets import QApplication, QTableWidgetItem, QLabel, QHeaderView
from PyQt5.QtGui import QColor, QPixmap
from PyQt5.QtCore import QSocketNotifier, QDateTime

import PyQt5.uic as uic

from Messages import CreateFlightMessage, UpdateFlightMessage, DeleteFlightMessage
from Messages import FlightCreatedMessage, FlightUpdatedMessage, FlightDeletedMessage

from Flight import Flight

class FlightDisplay(QApplication):
  def __init__(self, args):
    QApplication.__init__(self, args)

    # Colors to be used to indicate the kind of a flight.

    self._colors = [
      None,
      QColor(240, 200, 150),
      QColor(180, 240, 240)
    ]

    # Pixmaps to be used to indicate the kind of a flight.

    self._pixmaps = [
      QPixmap(),
      QPixmap('arrival.svg'),
      QPixmap('departure.svg'),
    ]

    # Add the "--gate/-g" option.

    parser = argparse.ArgumentParser(description='Flight display.')

    parser.add_argument('-g', '--gate')

    args = parser.parse_args(args[1:])

    self._gate = args.gate

    self._flights = { }

    # Load the user interface.

    self._ui = uic.loadUi('FlightDisplay.ui')

    # Handle the --gate/-g option if it was given.

    my_name = 'FlightDisplay'

    if self._gate is not None:
      self._ui.setWindowTitle(self._ui.windowTitle() + ': ' + self._gate)

      my_name += ' ' + self._gate

    # Contact the MX master at localhost, and tell him my name.

    self._mx = MX(my_name = my_name, mx_host = MX.effectiveHost())

    # Register the message I want to subscribe to...

    self._flight_created_msg = self._mx.register(FlightCreatedMessage.name())
    self._flight_updated_msg = self._mx.register(FlightUpdatedMessage.name())
    self._flight_deleted_msg = self._mx.register(FlightDeletedMessage.name())

    # ... and subscribe to them.

    self._mx.subscribe(self._flight_created_msg, self._flight_created_handler)
    self._mx.subscribe(self._flight_updated_msg, self._flight_updated_handler)
    self._mx.subscribe(self._flight_deleted_msg, self._flight_deleted_handler)

    timeout = int(time.time()) + 1

    self._timer = self._mx.createTimer(timeout, self._handle_timer)

    # Set up a socket notifier for incoming socket data.

    self._notifier = QSocketNotifier(self._mx.connectionNumber(), QSocketNotifier.Read, self)
    self._notifier.activated.connect(self._mx_event_handler)

    self._ui.show()

  def _handle_timer(self, timer, time):
      text = datetime.datetime.fromtimestamp(time).strftime('%A %B %d')
      self._ui.date.setText(text)

      text = datetime.datetime.fromtimestamp(time).strftime('%H:%M:%S')
      self._ui.time.setText(text)

      self._mx.adjustTimer(self._timer, time + 1)

  def _flight_created_handler(self, fd, msg_type, msg_version, payload):
    ''' Handle a "FlightCreated" message. '''

    ident, callsign, kind, time, city, gate, remarks = FlightCreatedMessage.unpack(payload)

    self._flights[ident] = Flight(callsign, kind, time, city, gate, remarks)

    self._refresh()

  def _flight_updated_handler(self, fd, msg_type, msg_version, payload):
    ''' Handle a "FlightUpdated" message. '''

    ident, callsign, kind, time, city, gate, remarks = FlightUpdatedMessage.unpack(payload)

    if self._flights[ident].update(callsign, kind, time, city, gate, remarks):
      self._refresh()

  def _flight_deleted_handler(self, fd, msg_type, msg_version, payload):
    ''' Handle a "FlightDeleted" message. '''

    ident, = FlightDeletedMessage.unpack(payload)

    del self._flights[ident]

    self._refresh()

  def _compare_flights(self, i1, i2):
    ''' Compare two flights for sorting. '''

    f1 = self._flights[i1]
    f2 = self._flights[i2]

    if f1.time < f2.time:
      return -1
    elif f1.time > f2.time:
      return 1
    else:
      return 0

  def _refresh(self):
    ''' Refresh the flight display. '''

    self._row_to_ident = { }
    self._ident_to_row = { }

    self._ui.table.setRowCount(0)

    ordered_idents = sorted(self._flights.keys(), key = lambda x: self._flights[x].time)

    row = -1

    for ident in ordered_idents:
      if self._flights[ident].callsign == '' \
      or self._flights[ident].city == '' \
      or self._flights[ident].gate == '' \
      or self._flights[ident].kind == Flight.Unknown:
        continue

      if self._gate is not None and self._flights[ident].gate != self._gate:
        continue

      row += 1

      self._row_to_ident[row] = ident
      self._ident_to_row[ident] = row

      kind = self._flights[ident].kind
      color = self._colors[kind]

      self._ui.table.insertRow(row)

      item = QTableWidgetItem(self._flights[ident].callsign)
      item.setBackground(color)
      self._ui.table.setItem(row, 0, item)

      label = QLabel(self._ui.table)
      label.setPixmap(self._pixmaps[kind])
      label.setStyleSheet('background: "%s";' % (color.name() if color else 'none'))
      self._ui.table.setCellWidget(row, 1, label)

      item = QTableWidgetItem(self._flights[ident].city)
      item.setBackground(color)
      self._ui.table.setItem(row, 2, item)

      t = time.strftime("%H:%M", time.localtime(self._flights[ident].time))
      item = QTableWidgetItem(t)
      item.setBackground(color)
      self._ui.table.setItem(row, 3, item)

      item = QTableWidgetItem(self._flights[ident].gate)
      item.setBackground(color)
      self._ui.table.setItem(row, 4, item)

      item = QTableWidgetItem(self._flights[ident].remarks)
      item.setBackground(color)
      self._ui.table.setItem(row, 5, item)

    for col in range(0, 5):
      self._ui.table.resizeColumnToContents(col)

  def _mx_event_handler(self):
    ''' Incoming data on the MX socket. Let MX handle it. '''

    r = self._mx.processEvents()

    if r > 0:     # Everything in order, wait for the next event(s)
      pass
    elif r == 0:  # No more events expected, just quit.
      self.exit(0)
    elif r < 0:   # Error, print message and quit.
      print(self._mx.error())
      self.exit(1)

if __name__ == '__main__':
  app = FlightDisplay(sys.argv)

  app.exec_()
