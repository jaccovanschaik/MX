#!/usr/bin/env python
# -*- coding: utf-8 -*-

'''
  hexdump.py: Hexdump function.

  Author:	Jacco van Schaik (jacco.van.schaik@dnw.aero)
  Copyright:	(c) 2011 DNW German-Dutch Windtunnels
  Created:	2011-03-09
  Version:	$Id: hexdump.py 426 2017-05-26 11:03:08Z jacco $
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
