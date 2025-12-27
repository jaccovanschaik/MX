# Makefile: Top-level makefile.
#
# Copyright:	(c) 2014-2025 Jacco van Schaik (jacco@jaccovanschaik.net)
# Version:	$Id: Makefile 457 2020-10-22 10:40:07Z jacco $
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.

JVS_TOP = $(HOME)

all clean install:
	$(MAKE) -C src $@
	$(MAKE) -C doc $@
	$(MAKE) -C python3 $@

update:
	git stash push
	git pull
	-git stash pop
	make install

tags:
	ctags --c-kinds=+p -R . \
            $(JVS_TOP)/Projects/libjvs \
            $(JVS_TOP)/include \
            /usr/include

.PHONY: tags
