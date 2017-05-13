# tests/test1/test.mk: Makefile fragment for test1.
#
# Copyright:	(c) 2014 Jacco van Schaik (jacco@jaccovanschaik.net)
# Version:	$Id: test.mk 325 2016-07-19 12:22:05Z jacco $
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.

TEST1_DIR  := tests/test1
TEST1_PING := $(TEST1_DIR)/ping
TEST1_ECHO := $(TEST1_DIR)/echo
TEST1_OBS  := $(TEST1_DIR)/observer
TEST1_LOG  := $(TEST1_DIR)/*.log

TEST1_OUTPUT := $(TEST1_DIR)/output.test
BASE1_OUTPUT := $(TEST1_DIR)/output.base

TESTS += test1
BASES += base1
CLEAN += $(TEST1_PING) $(TEST1_OBS) $(TEST1_ECHO) $(TEST1_OUTPUT) $(TEST1_LOG)

test1: $(TEST1_OUTPUT)
	diff $(TEST1_OUTPUT) $(BASE1_OUTPUT)

base1: $(TEST1_OUTPUT)
	cp $(TEST1_OUTPUT) $(BASE1_OUTPUT)

$(TEST1_OUTPUT): mx $(TEST1_PING) $(TEST1_OBS) $(TEST1_ECHO)
	./mx master -d
	export LC_ALL=C; $(TEST1_OBS) | sort > $(TEST1_OUTPUT) &
	$(TEST1_ECHO) &
	$(TEST1_PING)
	./mx quit
	sleep 1
