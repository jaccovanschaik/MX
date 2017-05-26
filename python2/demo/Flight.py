#!/usr/bin/env python
# -*- coding: utf-8 -*-

'''
  Flight.py: Defines a flight.

  Copyright: (c) 2016 Jacco van Schaik (jacco@jaccovanschaik.net)
  Created:   2016-08-03
  Version:   $Id: Flight.py 409 2017-04-04 10:20:06Z jacco $

  This software is distributed under the terms of the MIT license. See
  http://www.opensource.org/licenses/mit-license.php for details.
'''

class Flight(object):
  Unknown = 0
  Arrival = 1
  Departure = 2

  def __init__(self, callsign = None, kind = None, time = None,
                     city = None, gate = None, remarks = None):
    ''' Initialize this flight, using the given attributes. '''

    self.callsign = '' if callsign is None else callsign
    self.kind = Flight.Unknown if kind is None else kind
    self.time = 0 if time is None else time
    self.city = '' if city is None else city
    self.gate = '' if gate is None else gate
    self.remarks = '' if remarks is None else remarks

  def update(self, callsign = None, kind = None, time = None,
                     city = None, gate = None, remarks = None):
    ''' Update this flight, using any of the given attributes. Returns True if the flight did
        actually change, otherwise returns False. '''

    changed = False

    if callsign is not None and callsign != self.callsign:
      self.callsign = callsign
      changed = True

    if kind is not None and kind != self.kind:
      self.kind = kind
      changed = True

    if time is not None and time != self.time:
      self.time = time
      changed = True

    if city is not None and city != self.city:
      self.city = city
      changed = True

    if gate is not None and gate != self.gate:
      self.gate = gate
      changed = True

    if remarks is not None and remarks != self.remarks:
      self.remarks = remarks
      changed = True

    return changed

  def __repr__(self):
    ''' Returns a textual representation of this flight. '''

    if self.kind == Flight.Unknown:
      kind = "Flight.Unknown"
    elif self.kind == Flight.Arrival:
      kind = "Flight.Arrival"
    elif self.kind == Flight.Departure:
      kind = "Flight.Departure"

    text  = 'Flight(callsign = "%s"' % self.callsign
    text += ', kind = %s' % kind
    text += ', time = %f' % self.time
    text += ', city = "%s"' % self.city
    text += ', gate = "%s"' % self.gate
    text += ', remarks = "%s")' % self.remarks

    return text
