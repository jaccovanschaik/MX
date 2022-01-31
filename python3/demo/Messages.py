#!/usr/bin/env python3
# -*- coding: utf-8 -*-

'''
  Messages.py: Defines the application messages exchanged between components.

  Copyright: (c) 2016-2022 Jacco van Schaik (jacco@jaccovanschaik.net)
  Created:   2016-08-03
  Version:   $Id: Messages.py 461 2022-01-31 09:02:30Z jacco $

  This software is distributed under the terms of the MIT license. See
  http://www.opensource.org/licenses/mit-license.php for details.
'''

from Pack import Pack

class CreateFlightMessage(object):
  ''' A request to the FlightDB component to create a flight. '''

  @staticmethod
  def name():
    ''' Return the name of this message. '''

    return 'CreateFlight'

  @staticmethod
  def pack(callsign, kind, time, city, gate, remarks):
    ''' Return a packed CreateFlightMessage with the given parameters. '''

    return Pack.pack(
        Pack.STRING, callsign,
        Pack.UINT8,  kind,
        Pack.DOUBLE, time,
        Pack.STRING, city,
        Pack.STRING, gate,
        Pack.STRING, remarks)

  @staticmethod
  def unpack(payload):
    ''' Return a generator giving the unpacked fields from <payload>. '''

    return Pack.unpack(payload,
        Pack.STRING,
        Pack.UINT8,
        Pack.DOUBLE,
        Pack.STRING,
        Pack.STRING,
        Pack.STRING)

class UpdateFlightMessage(object):
  ''' A request to the FlightDB component to update a flight. '''

  @staticmethod
  def name():
    ''' Return the name of this message. '''

    return 'UpdateFlight'

  @staticmethod
  def pack(ident, callsign, kind, time, city, gate, remarks):
    ''' Return a packed UpdateFlightMessage with the given parameters. '''

    return Pack.pack(
        Pack.UINT32, ident,
        Pack.STRING, callsign,
        Pack.UINT8,  kind,
        Pack.DOUBLE, time,
        Pack.STRING, city,
        Pack.STRING, gate,
        Pack.STRING, remarks)

  @staticmethod
  def unpack(payload):
    ''' Return a generator giving the unpacked fields from <payload>. '''

    return Pack.unpack(payload,
        Pack.UINT32,
        Pack.STRING,
        Pack.UINT8,
        Pack.DOUBLE,
        Pack.STRING,
        Pack.STRING,
        Pack.STRING)

class DeleteFlightMessage(object):
  ''' A request to the FlightDB component to delete a flight. '''

  @staticmethod
  def name():
    ''' Return the name of this message. '''

    return 'DeleteFlight'

  @staticmethod
  def pack(ident):
    ''' Return a packed DeleteFlightMessage with the given parameters. '''

    return Pack.pack(Pack.UINT32, ident)

  @staticmethod
  def unpack(payload):
    ''' Return a generator giving the unpacked fields from <payload>. '''

    return Pack.unpack(payload, Pack.UINT32)

class FlightCreatedMessage(object):
  ''' A message from the FlightDB component informing others of a created flight. '''

  @staticmethod
  def name():
    ''' Return the name of this message. '''

    return 'FlightCreated'

  @staticmethod
  def pack(ident, callsign, kind, time, city, gate, remarks):
    ''' Return a packed FlightCreatedMessage with the given parameters. '''

    return Pack.pack(
        Pack.UINT32, ident,
        Pack.STRING, callsign,
        Pack.UINT8,  kind,
        Pack.DOUBLE, time,
        Pack.STRING, city,
        Pack.STRING, gate,
        Pack.STRING, remarks)

  @staticmethod
  def unpack(payload):
    ''' Return a generator giving the unpacked fields from <payload>. '''

    return Pack.unpack(payload,
        Pack.UINT32,
        Pack.STRING,
        Pack.UINT8,
        Pack.DOUBLE,
        Pack.STRING,
        Pack.STRING,
        Pack.STRING)

class FlightUpdatedMessage(object):
  ''' A message from the FlightDB component informing others of an updated flight. '''

  @staticmethod
  def name():
    ''' Return the name of this message. '''

    return 'FlightUpdated'

  @staticmethod
  def pack(ident, callsign, kind, time, city, gate, remarks):
    ''' Return a packed FlightUpdatedMessage with the given parameters. '''

    return Pack.pack(
        Pack.UINT32, ident,
        Pack.STRING, callsign,
        Pack.UINT8,  kind,
        Pack.DOUBLE, time,
        Pack.STRING, city,
        Pack.STRING, gate,
        Pack.STRING, remarks)

  @staticmethod
  def unpack(payload):
    ''' Return a generator giving the unpacked fields from <payload>. '''

    return Pack.unpack(payload,
        Pack.UINT32,
        Pack.STRING,
        Pack.UINT8,
        Pack.DOUBLE,
        Pack.STRING,
        Pack.STRING,
        Pack.STRING)

class FlightDeletedMessage(object):
  ''' A message from the FlightDB component informing others of a deleted flight. '''

  @staticmethod
  def name():
    ''' Return the name of this message. '''

    return 'FlightDeleted'

  @staticmethod
  def pack(ident):
    ''' Return a packed FlightDeletedMessage with the given parameters. '''

    return Pack.pack(Pack.UINT32, ident)

  @staticmethod
  def unpack(payload):
    ''' Return a generator giving the unpacked fields from <payload>. '''

    return Pack.unpack(payload, Pack.UINT32)

if __name__ == '__main__':
  import time
  from hexdump import hexdump

  msg = CreateFlightMessage.pack('KL1234', 1, time.time(), 'Frankfurt', 'D31', 'Boarding')

  print(hexdump(msg))

  print(list(CreateFlightMessage.unpack(msg)))
