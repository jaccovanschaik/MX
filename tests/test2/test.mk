# test2/test.mk: Description
#
# Copyright:	(c) 2013 Jacco van Schaik (jacco@jaccovanschaik.net)
# Version:	$Id: test.mk 122 2013-10-16 13:15:03Z jacco $
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.

TEST2_DIR := tests/test2
TEST2_DRIVER := $(TEST2_DIR)/mx_test_driver
TEST2_OBJECT := $(TEST2_DIR)/mx_test_object

TEST2_LOG  := $(TEST2_DIR)/test2.log
TEST2_BASE := $(TEST2_DIR)/test2.base

TESTS += test2
CLEAN += $(TEST2_DRIVER) $(TEST2_OBJECT) $(TEST2_LOG)
BASES += $(TEST2_BASE)

$(TEST2_LOG): $(TEST2_DRIVER) $(TEST2_OBJECT)
	./mx master
	$(TEST2_DRIVER) 51000 &
	sleep 0.1
	$(TEST2_OBJECT) 51000 > $(TEST2_LOG)
	sleep 0.1
	./mx quit

$(TEST2_BASE): $(TEST2_LOG)
	cp $< $@

test2: $(TEST2_LOG)
	diff $(TEST2_BASE) $<
