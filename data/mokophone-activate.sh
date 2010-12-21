#!/bin/bash

METHOD=$1
PARAMS=$2

if [ "$1" == "" ]; then METHOD="Frontend"; fi
if [ "$2" == "" ]; then PARAMS="string:phone"; fi

dbus-send --session --dest=org.mokosuite.phone --type=method_call --print-reply /org/mokosuite/Phone org.mokosuite.Phone.$METHOD $PARAMS
