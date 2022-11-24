#!/usr/bin/env python
# -*- coding: utf-8 -*-

'''
  hexdump.py: Hexdump function.

  Copyright: (c) 2011-2022 Jacco van Schaik (jacco@jaccovanschaik.net)
  Created:   2011-03-09

  This software is distributed under the terms of the MIT license. See
  http://www.opensource.org/licenses/mit-license.php for details.
'''

__version__ = '$Revision: 426 $'

def hexdump(buf, fd = None, indent = 0):
  '''
  Return (if fd is None) or print (to fd if it isn't) a hexdump of the data in <buf>, at indent
  level <indent>.
  '''

  assert isinstance(buf, bytes)

  def GetPrintableChar(c):
    '''
    Get a printable representation of character <c>.
    '''

    if c >= 32 and c <= 127:
      return chr(c)
    else:
      return '.'

  length = len(buf)

  i = 0

  string = ''

  while i < length:
    if length - i > 16:
      l = 16
    else:
      l = length - i

    string += '%s%05x  ' % ('  ' * indent, i)
    s = ' '.join( [ ("%02x" % c) for c in buf[i:i + l] ] )
    string += s
    sp = 49 - len(s)
    string += sp * ' '
    s = ''.join(["%c" % GetPrintableChar(c) for c in buf[i:i + l]])
    string += s + '\n'

    i = i + 16

  if fd:
    fd.write(string)
  else:
    return string
