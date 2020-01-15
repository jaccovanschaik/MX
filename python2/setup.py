#!/usr/bin/env python2
# -*- coding: utf-8 -*-

'''
  setup.py: Description
 
  Copyright: (c) 2016 Jacco van Schaik (jacco@jaccovanschaik.net)
  Created:   2016-07-06
  Version:   $Id: setup.py 450 2020-01-06 09:14:51Z jacco $
 
  This software is distributed under the terms of the MIT license. See
  http://www.opensource.org/licenses/mit-license.php for details.
'''

from distutils.core import setup, Extension

import os

EXTRA_TOP = os.environ['HOME']
EXTRA_INC = os.path.join(EXTRA_TOP, 'include')
EXTRA_LIB = os.path.join(EXTRA_TOP, 'lib')

setup(
  name="mx",
  version="1.0",
  description='Message Exchange',
  author='Jacco van Schaik',
  author_email='jacco@jaccovanschaik.net',
  ext_modules=[
    Extension("mx",
      sources      = [ "pymx.c" ],
      include_dirs = [ EXTRA_INC ],
      library_dirs = [ EXTRA_LIB ],
      libraries    = [ 'jvs', 'mx' ]
    ),
  ]
)
