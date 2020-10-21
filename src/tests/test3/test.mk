# tests/test3/test.mk: Makefile fragment for test3.
#
# Copyright:	(c) 2014 Jacco van Schaik (jacco@jaccovanschaik.net)
# Version:	$Id: test.mk 451 2020-10-21 21:15:06Z jacco $
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.

TEST3_DIR  := tests/test3
TEST3_PROD := $(TEST3_DIR)/producer
TEST3_CONS := $(TEST3_DIR)/consumer
TEST3_LOG  := $(TEST3_DIR)/*.log

TEST3_OUTPUT := $(TEST3_DIR)/consumer.test
BASE3_OUTPUT := $(TEST3_DIR)/consumer.base

TESTS += test3
BASES += base3
CLEAN += $(TEST3_PROD) $(TEST3_CONS) $(TEST3_OUTPUT) $(TEST3_LOG)

test3: $(TEST3_OUTPUT)
	diff $(TEST3_OUTPUT) $(BASE3_OUTPUT)

base3: $(TEST3_OUTPUT)
	cp $(TEST3_OUTPUT) $(BASE3_OUTPUT)

$(TEST3_OUTPUT): mx $(TEST3_PROD) $(TEST3_CONS)
	./mx master -b
	$(TEST3_PROD) &
	sleep 1
	$(TEST3_CONS) 0 > $(TEST3_DIR)/consumer0.test &
	sleep 1
	$(TEST3_CONS) 1 > $(TEST3_DIR)/consumer1.test &
	sleep 1
	$(TEST3_CONS) 2 > $(TEST3_DIR)/consumer2.test &
	sleep 1
	$(TEST3_CONS) 3 > $(TEST3_DIR)/consumer3.test &
	sleep 1
	$(TEST3_CONS) 4 > $(TEST3_DIR)/consumer4.test &
	sleep 1
	$(TEST3_CONS) 5 > $(TEST3_DIR)/consumer5.test &
	sleep 1
	$(TEST3_CONS) 6 > $(TEST3_DIR)/consumer6.test &
	sleep 1
	$(TEST3_CONS) 7 > $(TEST3_DIR)/consumer7.test &
	sleep 1
	$(TEST3_CONS) 8 > $(TEST3_DIR)/consumer8.test &
	sleep 1
	$(TEST3_CONS) 9 > $(TEST3_DIR)/consumer9.test &
	sleep 1
	./mx quit
	sleep 1
	cat $(TEST3_DIR)/consumer?.test > $(TEST3_OUTPUT)
	rm $(TEST3_DIR)/consumer?.test
