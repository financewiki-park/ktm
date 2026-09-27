#!/bin/sh
# Name: ktm
# Author: financewiki-park
# Icon: /mnt/us/ktm-icon.png
# ktm-managed-scriptlet-v1
# ktm opens the installed official KTerm through KPM.
KPM=/var/local/kmc/bin/kpm
exec "$KPM" launch ktm
