#!/bin/sh

# script.sh: Run the demo application.
#
# Author:    Jacco van Schaik (jacco.van.schaik@dnw.aero)
# Copyright: (c) 2016 DNW German-Dutch Windtunnels
# Created:   2016-08-04
# Version:   $Id: script.sh 426 2017-05-26 11:03:08Z jacco $

mx master -d

echo "The MX name is:" `mx name`
echo "The MX port is:" `mx port`
echo "\nList the participating components with 'mx list'"
echo "Show the current version of MX with 'mx version'"
echo "\nUse 'mx quit' to stop the demo."

python3 FlightDB.py &
python3 FlightEditor.pyw &
python3 FlightDisplay.pyw &
python3 FlightDisplay.pyw --gate D29 &
python3 FlightDisplay.pyw --gate D31 &
python3 FlightDisplay.pyw --gate B11 &
python3 FlightDisplay.pyw --gate B12 &
