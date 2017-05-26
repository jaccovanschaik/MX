#!/bin/sh

# script.sh: Run the demo application.
#
# Author:    Jacco van Schaik (jacco.van.schaik@dnw.aero)
# Copyright: (c) 2016 DNW German-Dutch Windtunnels
# Created:   2016-08-04
# Version:   $Id: script.sh 413 2017-04-04 20:02:22Z jacco $

mx master -d

echo "The MX name is:" `mx name`
echo "The MX port is:" `mx port`
echo "\nList the participating components with 'mx list'"
echo "Show the current version of MX with 'mx version'"
echo "\nUse 'mx quit' to stop the demo."

python FlightDB.py &
python FlightEditor.pyw &
python FlightDisplay.pyw &
python FlightDisplay.pyw --gate D29 &
python FlightDisplay.pyw --gate D31 &
python FlightDisplay.pyw --gate B11 &
python FlightDisplay.pyw --gate B12 &
