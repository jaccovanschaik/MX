# Makefile: Description
#
# Copyright:	(c) 2014 Jacco van Schaik (jacco@jaccovanschaik.net)
# Version:	$Id: Makefile 377 2016-08-23 13:17:55Z jacco $
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.

PNG = Connection.png \
      Threads.png \
      MsgCancelUpdate.png \
      MsgHeader.png \
      MsgHelloRequest.png \
      MsgHelloReply.png \
      MsgHelloReport.png \
      MsgHelloUpdate.png \
      MsgQuitRequest.png \
      MsgRegisterReply.png \
      MsgRegisterReport.png \
      MsgRegisterRequest.png \
      MsgSubscribeUpdate.png

SVG = $(patsubst %.png,%.svg,$(PNG))

all install: $(PNG)

%.png: %.svg
	-inkscape --export-area-drawing --export-dpi=150 --export-png=$@ $<

clean:
	rm -f $(PNG)
