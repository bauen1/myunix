#!/bin/sh
set -e -x
rm -f misc/switch.pdump
mkfifo misc/switch.pdump
vde_switch -f misc/switch.conf -s misc/switch
