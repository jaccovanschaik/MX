# tests.mk: Description
#
# Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
# Version:	$Id: tests.mk 119 2013-10-15 08:55:49Z jacco $
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.

SUBS := tests/test1 tests/test2 tests/test3

include $(patsubst %, %/test.mk, $(SUBS))
