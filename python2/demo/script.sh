#!/bin/sh

# script.sh: Run the demo application.
#
# Author:    Jacco van Schaik (jacco.van.schaik@dnw.aero)
# Copyright: (c) 2016 DNW German-Dutch Windtunnels
# Created:   2016-08-04
# Version:   $Id: script.sh 447 2020-01-06 07:55:00Z jacco $

mx master -d

echo -e "The MX name is:" `mx name`
echo -e "The MX port is:" `mx port`
echo -e "\nList the participating components with 'mx list'"
echo -e "Show the current version of MX with 'mx version'"
echo -e "\nUse 'mx quit' to stop the demo."

python FlightDB.py &
python FlightEditor.pyw &
python FlightDisplay.pyw &
python FlightDisplay.pyw --gate D29 &
python FlightDisplay.pyw --gate D31 &
python FlightDisplay.pyw --gate B11 &
python FlightDisplay.pyw --gate B12 &
