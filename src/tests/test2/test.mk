# tests/test2/test.mk: Description
#
# Copyright:	(c) 2014 Jacco van Schaik (jacco@jaccovanschaik.net)
# Version:	$Id: test.mk 324 2016-07-19 09:58:04Z jacco $
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.

TEST2_DIR  := tests/test2
TEST2_PING := $(TEST2_DIR)/ping
TEST2_ECHO := $(TEST2_DIR)/echo
TEST2_OBS  := $(TEST2_DIR)/observer
TEST2_LOG  := $(TEST2_DIR)/*.log

TEST2_OUTPUT := $(TEST2_DIR)/output.test
BASE2_OUTPUT := $(TEST2_DIR)/output.base

TESTS += test2
BASES += base2
CLEAN += $(TEST2_PING) $(TEST2_OBS) $(TEST2_ECHO) $(TEST2_OUTPUT) $(TEST2_LOG)

test2: $(TEST2_OUTPUT)
	diff $(TEST2_OUTPUT) $(BASE2_OUTPUT)

base2: $(TEST2_OUTPUT)
	cp $(TEST2_OUTPUT) $(BASE2_OUTPUT)

$(TEST2_OUTPUT): mx $(TEST2_PING) $(TEST2_OBS) $(TEST2_ECHO)
	export LC_ALL=C; $(TEST2_OBS) | sort > $(TEST2_OUTPUT) &
	sleep 1
	$(TEST2_ECHO) &
	$(TEST2_PING)
	./mx quit
	sleep 1
