# test1/test.mk: Description
#
# Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
# Version:	$Id: test.mk 108 2013-10-14 18:31:33Z jacco $
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.

TEST1_DIR := tests/test1
TEST1_EXE := $(TEST1_DIR)/test1

TESTS += test1
CLEAN += $(TEST1_EXE)

test1: $(TEST1_EXE) $(TEST1_DIR)/test.sh
	$(TEST1_DIR)/test.sh
