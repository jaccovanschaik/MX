#!/usr/bin/env python3
# -*- coding: utf-8 -*-

'''
  Pack.py: Message packer/unpacker.

  Copyright: (c) 2016 Jacco van Schaik (jacco@jaccovanschaik.net)
  Created:   2016-08-03
  Version:   $Id: Pack.py 426 2017-05-26 11:03:08Z jacco $

  This software is distributed under the terms of the MIT license. See
  http://www.opensource.org/licenses/mit-license.php for details.
'''

import struct

class Pack(object):
  ''' Data packer/unpacker. '''

  # The field types we can pack.

  INT8    = 0     # 8-bit signed integer
  UINT8   = 1     # 8-bit unsigned integer
  INT16   = 2     # 16-bit signed integer
  UINT16  = 3     # 16-bit unsigned integer
  INT32   = 4     # 32-bit signed integer
  UINT32  = 5     # 32-bit unsigned integer
  INT64   = 6     # 64-bit signed integer
  UINT64  = 7     # 64-bit unsigned integer
  FLOAT   = 8     # 32-bit floating point
  DOUBLE  = 9     # 64-bit floating point
  STRING  = 10    # Character string

  # Internal data about the numerical field types.

  _items = {
    INT8:   { 'fmt': 'b', 'size': 1, 'type': int },
    UINT8:  { 'fmt': 'B', 'size': 1, 'type': int },
    INT16:  { 'fmt': 'h', 'size': 2, 'type': int },
    UINT16: { 'fmt': 'H', 'size': 2, 'type': int },
    INT32:  { 'fmt': 'i', 'size': 4, 'type': int },
    UINT32: { 'fmt': 'I', 'size': 4, 'type': int },
    INT64:  { 'fmt': 'q', 'size': 8, 'type': int },
    UINT64: { 'fmt': 'Q', 'size': 8, 'type': int },
    FLOAT:  { 'fmt': 'f', 'size': 4, 'type': float },
    DOUBLE: { 'fmt': 'd', 'size': 8, 'type': float },
  }

  @staticmethod
  def pack(*args):
    ''' Pack the given list of parameters (which consists of alternating field types as given above,
        and field values) and return the resulting byte string. '''

    count = len(args)
    index = 0
    buf = b''

    while index < count:
      spec  = args[index]
      value = args[index + 1]

      if spec == Pack.STRING:
        buf += struct.pack('>I', len(value)) + value.encode('utf-8')
        index += 2
      elif spec in Pack._items:
        buf += struct.pack('>' + Pack._items[spec]['fmt'], value)
        index += 2
      else:
        raise Exception("unknown spec (%d)" % spec)

    return buf

  @staticmethod
  def unpack(payload, *args):
    ''' Unpack <payload>, using the list of field types given in <args>. '''

    count = len(args)
    index = 0
    offset = 0

    while index < count:
      spec = args[index]

      if spec == Pack.STRING:
        length, = struct.unpack('>I', payload[offset:offset+4])
        yield payload[offset + 4:offset + 4 + length].decode('utf-8')
        index += 1
        offset += 4 + length
      elif spec in Pack._items:
        length = Pack._items[spec]['size']
        yield struct.unpack('>' + Pack._items[spec]['fmt'], payload[offset:offset+length])[0]
        index += 1
        offset += length
      else:
        raise Exception("unknown spec (%d)" % spec)

if __name__ == '__main__':
  from hexdump import hexdump

  buf = Pack.pack(
      Pack.INT8,    8,
      Pack.UINT8,   8,
      Pack.INT16,   16,
      Pack.UINT16,  16,
      Pack.INT32,   32,
      Pack.UINT32,  32,
      Pack.INT64,   64,
      Pack.UINT64,  64,
      Pack.FLOAT,   3.14159,
      Pack.DOUBLE,  2.71828,
      Pack.STRING,  'Dit is een string')

  print(hexdump(buf))

  data = Pack.unpack(buf,
      Pack.INT8,
      Pack.UINT8,
      Pack.INT16,
      Pack.UINT16,
      Pack.INT32,
      Pack.UINT32,
      Pack.INT64,
      Pack.UINT64,
      Pack.FLOAT,
      Pack.DOUBLE,
      Pack.STRING)

  print(list(data))
