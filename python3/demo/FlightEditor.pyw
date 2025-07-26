#!/usr/bin/env python3
# -*- coding: utf-8 -*-

'''
  FlightEditor.pyw: Editor for flights.

  Copyright: (c) 2016-2025 Jacco van Schaik (jacco@jaccovanschaik.net)
  Created:   2016-08-03

  This software is distributed under the terms of the MIT license. See
  http://www.opensource.org/licenses/mit-license.php for details.

  vim: softtabstop=4 shiftwidth=4 expandtab textwidth=100 columns=100
'''

import sys, time

from mx import MX

from PyQt5.QtWidgets import QApplication, QMenu, QTableWidgetItem, QComboBox, QDateTimeEdit
from PyQt5.QtCore import QSocketNotifier, QDateTime

import PyQt5.uic as uic

from Messages import CreateFlightMessage, UpdateFlightMessage, DeleteFlightMessage
from Messages import FlightCreatedMessage, FlightUpdatedMessage, FlightDeletedMessage

from Flight import Flight

class FlightEditor(QApplication):
  ''' An editor for flights. '''

  def __init__(self, args):
    ''' Initializer. '''

    QApplication.__init__(self, args)

    # A dict of the flights I know.

    self._flights = { }

    # Load the user interface.

    self._ui = uic.loadUi('FlightEditor.ui')

    # Set up handlers for right-clicks and changes of cells' contents.

    self._ui.table.customContextMenuRequested.connect(self._context_menu_requested)
    self._ui.table.cellChanged.connect(self._cell_changed)

    # Adjust column widths.

    self._ui.table.resizeColumnsToContents()

    # Contact the MX master at localhost, and tell him my name is "FlightEditor".

    self._mx = MX(my_name = 'FlightEditor', mx_host = MX.effectiveHost())

    # Register the messages I'm going to send,

    self._create_flight_msg = self._mx.register(CreateFlightMessage.name())
    self._update_flight_msg = self._mx.register(UpdateFlightMessage.name())
    self._delete_flight_msg = self._mx.register(DeleteFlightMessage.name())

    # Register the messages I'm going to subscribe to,

    self._flight_created_msg = self._mx.register(FlightCreatedMessage.name())
    self._flight_updated_msg = self._mx.register(FlightUpdatedMessage.name())
    self._flight_deleted_msg = self._mx.register(FlightDeletedMessage.name())

    # ... and subscribe to them.

    self._mx.subscribe(self._flight_created_msg, self._flight_created_handler)
    self._mx.subscribe(self._flight_updated_msg, self._flight_updated_handler)
    self._mx.subscribe(self._flight_deleted_msg, self._flight_deleted_handler)

    # Set up a notifier for incoming data on the MX socket.

    self._notifier = QSocketNotifier(self._mx.connectionNumber(), QSocketNotifier.Read, self)
    self._notifier.activated.connect(self._mx_event_handler)

    # Show the UI window.

    self._ui.show()

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

    args = list(FlightDeletedMessage.unpack(payload))

    ident = args[0]

    del self._flights[ident]

    self._refresh()

  def _compare_flights(self, i1, i2):
    ''' Compare flights for sorting. '''

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

    for row, ident in enumerate(ordered_idents):
      self._row_to_ident[row] = ident
      self._ident_to_row[ident] = row

      self._ui.table.insertRow(row)

      box = QComboBox(self._ui.table)

      box.addItem('')
      box.addItem('Arrival')
      box.addItem('Departure')

      box.setCurrentIndex(self._flights[ident].kind)

      box.currentIndexChanged.connect(lambda index, ident = ident: self._kind_changed(ident, index))

      datetime = QDateTime()
      datetime.setTime_t(int(self._flights[ident].time))

      w = QDateTimeEdit(datetime, self._ui.table)

      w.setDisplayFormat('yyyy-MM-dd hh:mm:ss')
      w.dateTimeChanged.connect(lambda qtime, ident = ident: self._time_changed(ident, qtime))

      self._ui.table.setItem(row, 0, QTableWidgetItem(self._flights[ident].callsign))
      self._ui.table.setCellWidget(row, 1, box)
      self._ui.table.setItem(row, 2, QTableWidgetItem(self._flights[ident].city))
      self._ui.table.setCellWidget(row, 3, w)
      self._ui.table.setItem(row, 4, QTableWidgetItem(self._flights[ident].gate))
      self._ui.table.setItem(row, 5, QTableWidgetItem(self._flights[ident].remarks))

    self._ui.table.resizeColumnsToContents()
    self._ui.table.horizontalHeader().setStretchLastSection(True)

  def _send_flight_update(self, ident):
    ''' Broadcast a "FlightUpdate" message for the flight with ident <ident>. '''

    flight = self._flights[ident]

    payload = UpdateFlightMessage.pack(ident,
        flight.callsign, flight.kind, flight.time, flight.city, flight.gate, flight.remarks)

    self._mx.broadcast(self._update_flight_msg, 0, payload)

  def _kind_changed(self, ident, kind):
    ''' The "kind" of a flight was changed. Handle this. '''

    self._flights[ident].kind = kind

    self._send_flight_update(ident)

  def _time_changed(self, ident, datetime):
    ''' The arrival/departure time of a flight was changed. Handle this. '''

    self._flights[ident].time = datetime.toTime_t()

    self._send_flight_update(ident)

  def _context_menu_requested(self, point):
    ''' A context menu was requested. Handle this. '''

    row = self._ui.table.rowAt(point.y())

    menu = QMenu(self._ui.table)

    add_action = menu.addAction('Create flight')
    del_action = menu.addAction('Delete flight')

    add_action.setEnabled(True)
    del_action.setEnabled(row != -1)

    action = menu.exec_(self._ui.table.viewport().mapToGlobal(point))

    if action == add_action:
      payload = CreateFlightMessage.pack('', 0, time.time(), '', '', '')

      self._mx.broadcast(self._create_flight_msg, 0, payload)
    elif action == del_action:
      ident = self._row_to_ident[row]

      payload = DeleteFlightMessage.pack(ident)

      self._mx.broadcast(self._delete_flight_msg, 0, payload)

  def _cell_changed(self, row, col):
    ''' The contents of the cell at row <row> and column <col> was changed. Handle this. '''

    ident = self._row_to_ident[row]
    flight = self._flights[ident]

    if col == 0:
      new_callsign = str(self._ui.table.item(row, col).text())
      if flight.update(callsign = new_callsign):
        self._send_flight_update(ident)
    elif col == 2:
      new_city = str(self._ui.table.item(row, col).text())
      if flight.update(city = new_city):
        self._send_flight_update(ident)
    elif col == 4:
      new_gate = str(self._ui.table.item(row, col).text())
      if flight.update(gate = new_gate):
        self._send_flight_update(ident)
    elif col == 5:
      new_remarks = str(self._ui.table.item(row, col).text())
      if flight.update(remarks = new_remarks):
        self._send_flight_update(ident)

    self._ui.table.resizeColumnsToContents()
    self._ui.table.horizontalHeader().setStretchLastSection(True)

  def _mx_event_handler(self):
    ''' Some data arrived on the MX socket. Let MX handle this. '''

    r = self._mx.processEvents()

    if r > 0:     # Everything in order, wait for the next event(s)
      pass
    elif r == 0:  # No more events expected, just quit.
      self.exit()
    elif r < 0:   # Error, print message and quit.
      print(self._mx.error())
      self.exit()

if __name__ == '__main__':
  app = FlightEditor(sys.argv)

  app.exec_()
