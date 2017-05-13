# tests/test4/test.mk: Makefile fragment for test4.
#
# Copyright:	(c) 2014 Jacco van Schaik (jacco@jaccovanschaik.net)
# Version:	$Id: test.mk 324 2016-07-19 09:58:04Z jacco $
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.

TEST4_DIR  := tests/test4
TEST4_PING := $(TEST4_DIR)/ping
TEST4_ECHO := $(TEST4_DIR)/echo
TEST4_OBS  := $(TEST4_DIR)/observer
TEST4_LOG  := $(TEST4_DIR)/*.log

TEST4_OUTPUT := $(TEST4_DIR)/output.test
BASE4_OUTPUT := $(TEST4_DIR)/output.base

TESTS += test4
BASES += base4
CLEAN += $(TEST4_PING) $(TEST4_OBS) $(TEST4_ECHO) $(TEST4_OUTPUT) $(TEST4_LOG)

test4: $(TEST4_OUTPUT)
	diff $(TEST4_OUTPUT) $(BASE4_OUTPUT)

base4: $(TEST4_OUTPUT)
	cp $(TEST4_OUTPUT) $(BASE4_OUTPUT)

$(TEST4_OUTPUT): mx $(TEST4_PING) $(TEST4_OBS) $(TEST4_ECHO)
	./mx master &
	sleep 1
	export LC_ALL=C; $(TEST4_OBS) | sort > $(TEST4_OUTPUT) &
	$(TEST4_ECHO) &
	$(TEST4_PING)
	./mx quit
	sleep 1
