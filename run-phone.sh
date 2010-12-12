#!/bin/bash

DISPLAY=":1"
if [ "$1" != "" ]; then
DISPLAY="$1"
fi

make &&
cd data && sudo make install && cd .. &&
(
if [ "$BUS" == "session" ]; then
DISPLAY=$DISPLAY ELM_FINGER=80 ELM_SCALE=2 ELM_THEME=gry src/mokophone
else
DISPLAY=$DISPLAY ELM_FINGER=80 ELM_SCALE=2 ELM_THEME=gry DBUS_SYSTEM_BUS_ADDRESS="tcp:host=neo,port=8000" src/mokophone
fi
)