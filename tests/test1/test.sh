#!/bin/sh

# tests/test1/test.sh: Description
#
# Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
# Version:	$Id: test.sh 108 2013-10-14 18:31:33Z jacco $
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.

DIR=`dirname $0`

trap "./mx quit" 0

./mx master

$DIR/test1
