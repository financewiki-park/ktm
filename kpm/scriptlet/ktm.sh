#!/bin/sh
# Name: ktm
# Author: financewiki-park
# Icon: /mnt/us/ktm-icon.png
# ktm-managed-scriptlet-v1
# KTerm is installed by KPM as the ktm package dependency.  Its -e option
# opens a visible Kindle terminal window and executes this KPM launch there.
KPM=/var/local/kmc/bin/kpm
exec "$KPM" launch kterm -e "$KPM launch ktm --ui"
