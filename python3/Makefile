# Makefile: Description
#
# Copyright: (c) 2016 Jacco van Schaik (jacco@jaccovanschaik.net)
# Created:   2016-07-06
# Version:   $Id: Makefile 430 2017-05-26 11:22:19Z jacco $
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.

PYTHON_TOP = $(HOME)
PYTHON_MOD = $(PYTHON_TOP)/.python

all:
	python3 setup.py build

install: all
	cp `find . -name mx.cpython-35m-x86_64-linux-gnu.so` $(PYTHON_MOD)

tags:
	ctags --c-kinds=+p -R /usr/include .

clean:
	rm -rf build core *.log
	make -C demo clean
