# Makefile: Top-level makefile.
#
# Copyright:	(c) 2014 Jacco van Schaik (jacco@jaccovanschaik.net)
# Version:	$Id: Makefile 430 2017-05-26 11:22:19Z jacco $
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.

all clean install:
	$(MAKE) -C src $@
	$(MAKE) -C doc $@
	$(MAKE) -C python3 $@
