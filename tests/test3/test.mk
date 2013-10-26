# test3/test.mk: Description
#
# Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
# Version:	$Id: test.mk 122 2013-10-16 13:15:03Z jacco $
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.

TEST3_DIR := tests/test3
TEST3_EXE := $(TEST3_DIR)/test3

TEST3_LOG   := $(TEST3_DIR)/test3.log
TEST3_LOG1  := $(TEST3_DIR)/test3-1.log
TEST3_LOG2  := $(TEST3_DIR)/test3-2.log
TEST3_LOG3  := $(TEST3_DIR)/test3-3.log

TEST3_BASE  := $(TEST3_DIR)/test3.base

TESTS += test3
CLEAN += $(TEST3_EXE) $(TEST3_LOG) $(TEST3_LOG1) $(TEST3_LOG2) $(TEST3_LOG3)
BASES += $(TEST3_BASE)

$(TEST3_LOG): $(TEST3_EXE)
	./mx master
	$(TEST3_EXE) 1 > $(TEST3_LOG1) 2>&1 &
	sleep 0.1
	$(TEST3_EXE) 2 > $(TEST3_LOG2) 2>&1 &
	sleep 0.1
	$(TEST3_EXE) 3 > $(TEST3_LOG3) 2>&1 &
	sleep 0.1
	./mx quit
	cat $(TEST3_LOG1) $(TEST3_LOG2) $(TEST3_LOG3) > $(TEST3_LOG)

$(TEST3_BASE): $(TEST3_LOG)
	cp $< $@

test3: $(TEST3_LOG)
	diff $(TEST3_BASE) $(TEST3_LOG)
