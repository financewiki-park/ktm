#!/bin/sh
# Name: ktm
# Author: financewiki-park
# Icon: /mnt/us/ktm-icon.png
# ktm-managed-scriptlet-v1
# ktm launches its patched KTerm directly so the menu and destination shell
# share one VTE instance.
KPM=/var/local/kmc/bin/kpm
exec "$KPM" launch ktm
