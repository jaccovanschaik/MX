# tests/test6/test.mk: Makefile fragment for test6.
#
# Copyright:	(c) 2014 Jacco van Schaik (jacco@jaccovanschaik.net)
# Version:	$Id: test.mk 405 2017-02-10 15:19:59Z jacco $
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.

TEST6_DIR  := tests/test6
TEST6_EXE  := $(TEST6_DIR)/test

TESTS += test6
CLEAN += $(TEST6_EXE)

test6: $(TEST6_EXE)
	$(TEST6_EXE)
