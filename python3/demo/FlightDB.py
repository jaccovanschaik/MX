#!/usr/bin/env python3
# -*- coding: utf-8 -*-

'''
  FlightDB.py: The "database" holding flights.

  Copyright: (c) 2016-2025 Jacco van Schaik (jacco@jaccovanschaik.net)
  Created:   2016-08-03
  Version:   $Id: FlightDB.py 461 2022-01-31 09:02:30Z jacco $

  This software is distributed under the terms of the MIT license. See
  http://www.opensource.org/licenses/mit-license.php for details.
'''

import os, pprint, time, sys

from mx import MX

from Flight import Flight

from Messages import CreateFlightMessage, UpdateFlightMessage, DeleteFlightMessage
from Messages import FlightCreatedMessage, FlightUpdatedMessage, FlightDeletedMessage

def handle_timer(timer, time):
    print("handle_timer: ", timer, time, file=sys.stderr)

    # mx.adjustTimer(timer, time + 1)

class FlightDB(object):
  def __init__(self):
    ''' Initialize the database. '''

    # Initialize a dict of flights.

    self._flights = { }

    # Contact the MX master at localhost, tell him my name is "FlightDB".

    self._mx = MX(mx_host = MX.effectiveHost(), my_name = 'FlightDB')

    timer = self._mx.createTimer(time.time() + 1, self._handle_timer)

    # print("Created timer:", timer)

    # Register the messages I want to subscribe to...

    self._create_flight_msg = self._mx.register(CreateFlightMessage.name())
    self._update_flight_msg = self._mx.register(UpdateFlightMessage.name())
    self._delete_flight_msg = self._mx.register(DeleteFlightMessage.name())

    # ... and then subscribe to them.

    self._mx.subscribe(self._create_flight_msg, self._create_flight_handler)
    self._mx.subscribe(self._update_flight_msg, self._update_flight_handler)
    self._mx.subscribe(self._delete_flight_msg, self._delete_flight_handler)

    # Register the messages I want to send out.

    self._flight_created_msg = self._mx.register(FlightCreatedMessage.name())
    self._flight_updated_msg = self._mx.register(FlightUpdatedMessage.name())
    self._flight_deleted_msg = self._mx.register(FlightDeletedMessage.name())

    # Tell me if anyone subscribes to "Flight Created" messages.

    self._mx.onNewSubscriber(self._flight_created_msg, self._new_subscriber_handler)

  def _handle_timer(self, timer, time):
    print("_handle_timer at time", time, file=sys.stderr)

    time += 1;

    print("_handle_timer: adjusting to time", time, file=sys.stderr)

    self._mx.adjustTimer(timer, time)

  def _next_ident(self):
    ''' Get the next ident for a new Flight. '''

    if not self._flights.keys():
      return 0
    else:
      return max(self._flights.keys()) + 1

  def _create_flight_handler(self, fd, msg_type, msg_version, payload):
    ''' Handle a "CreateFlight" message. '''

    # Unpack the data in the request.

    callsign, kind, time, city, gate, remarks = CreateFlightMessage.unpack(payload)

    # Get an ident for the Flight we're about to create.

    ident = self._next_ident()

    # Create the Flight...

    self._flights[ident] = Flight(callsign, kind, time, city, gate, remarks)

    # And broadcast a message to all "FlightCreated" subscribers.

    payload = FlightCreatedMessage.pack(ident, callsign, kind, time, city, gate, remarks)

    self._mx.broadcast(self._flight_created_msg, 0, payload)

  def _update_flight_handler(self, fd, msg_type, msg_version, payload):
    ''' Handle an "UpdateFlight" message. '''

    # Unpack the data from the message.

    ident, callsign, kind, time, city, gate, remarks = UpdateFlightMessage.unpack(payload)

    # Make sure this flight actually exists.

    assert ident in self._flights

    # Update the flight data, and if it actually changed send out the update
    # to all "FlightUpdated" subscribers,

    if self._flights[ident].update(callsign, kind, time, city, gate, remarks):
        payload = FlightUpdatedMessage.pack(ident, callsign, kind, time, city, gate, remarks)

        self._mx.broadcast(self._flight_updated_msg, 0, payload)

  def _delete_flight_handler(self, fd, msg_type, msg_version, payload):
    ''' Handle a "DeleteFlight" message. '''

    # Get the ident of the flight they want us to delete.

    args = list(DeleteFlightMessage.unpack(payload))

    ident = args[0]

    # Delete it...

    del self._flights[ident]

    # ... and send out a message to all "FlightDeleted" subscribers.

    payload = FlightDeletedMessage.pack(ident)

    self._mx.broadcast(self._flight_deleted_msg, 0, payload)

  def _new_subscriber_handler(self, fd, msg_type):
    ''' There's a new subscriber to "FlightCreated" messages. Handle this. '''

    # Send the new subscriber a "FlightCreated" message for every flight we know, so that they are
    # brought up to speed as soon as possible. Note that we're using send here instead of broadcast,
    # because we only need to inform this one subscriber.

    for ident, flight in self._flights.items():
      payload = FlightCreatedMessage.pack(ident,
          flight.callsign, flight.kind, flight.time, flight.city, flight.gate, flight.remarks)

      self._mx.send(fd, self._flight_created_msg, 0, payload)

  def run(self):
    ''' Run this MX component. '''

    self._mx.run()

  def load(self):
    ''' Load the set of flights from "flights.txt". '''

    try:
      with open("flights.txt", "r") as f:
        self._flights = eval(f.read())
    except Exception as e:
      print("Error while loading flights:", e)

  def save(self):
    ''' Save the set of flights to "flights.txt". '''

    with open("flights.txt", "w") as f:
      pprint.pprint(self._flights, f)
      # f.write(repr(self._flights) + os.linesep)

if __name__ == '__main__':
  app = FlightDB()

  app.load()
  app.run()
  app.save()
