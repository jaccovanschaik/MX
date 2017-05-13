# tests/test.mk: Makefile fragment to run the tests.
#
# Copyright:	(c) 2014 Jacco van Schaik (jacco@jaccovanschaik.net)
# Version:	$Id: tests.mk 405 2017-02-10 15:19:59Z jacco $
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.

SUBS := tests/test1 tests/test2 tests/test3 tests/test4 tests/test5 tests/test6

include $(patsubst %, %/test.mk, $(SUBS))
