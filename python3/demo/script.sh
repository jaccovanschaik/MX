#!/bin/sh

# script.sh: Run the demo application.
#
# Copyright: (c) 2016- 2022 Jacco van Schaik (jacco@jaccovanschaik.net)
# Created:   2016-08-04
#
# This software is distributed under the terms of the MIT license. See
# http://www.opensource.org/licenses/mit-license.php for details.

# export MX_HOST=home

# ssh -f home MX_HOME=$MX_HOME mx master

# sleep 1

mx master -b

printf "The MX name is: %s\n" $(mx name)
printf "The MX port is: %s\n" $(mx port)
printf "\nList the participating components with 'mx list'\n"
printf "Show the current version of MX with 'mx version'\n"
printf "\nUse 'mx quit' to stop the demo.\n"

python3 FlightDB.py &
python3 FlightEditor.pyw &
python3 FlightDisplay.pyw &

for letter in $(seq 10 11); do
    for number in $(seq 1 2); do
        gate=$(printf "%X%02d" $letter $number)
        python3 FlightDisplay.pyw --gate $gate &
    done
done
