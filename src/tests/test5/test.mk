# tests/test5/test.mk: Makefile fragment for test5.
#
# Copyright:	(c) 2014 Jacco van Schaik (jacco@jaccovanschaik.net)
# Version:	$Id: test.mk 451 2020-10-21 21:15:06Z jacco $
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.

TEST5_DIR  := tests/test5
TEST5_EXE  := $(TEST5_DIR)/test
TEST5_LOG  := $(TEST5_DIR)/*.log

TEST5_OUTPUT := $(TEST5_DIR)/output.test
BASE5_OUTPUT := $(TEST5_DIR)/output.base

TESTS += test5
BASES += base5
CLEAN += $(TEST5_EXE) $(TEST5_OUTPUT) $(TEST5_LOG)

test5: $(TEST5_OUTPUT)
	diff $(TEST5_OUTPUT) $(BASE5_OUTPUT)

base5: $(TEST5_OUTPUT)
	cp $(TEST5_OUTPUT) $(BASE5_OUTPUT)

$(TEST5_OUTPUT): mx $(TEST5_EXE)
	./mx master -b
	$(TEST5_EXE) > $(TEST5_OUTPUT)
	./mx quit
	sleep 1
