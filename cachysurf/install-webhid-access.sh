#!/bin/sh
set -eu

cat > /etc/udev/rules.d/70-cachysurf-webhid.rules <<'RULE'
# Cachy Surf WebHID: grant active local sessions access to hidraw devices.
KERNEL=="hidraw*", SUBSYSTEM=="hidraw", TAG+="uaccess"
RULE

udevadm control --reload-rules
udevadm trigger --subsystem-match=hidraw
printf '%s\n' 'Cachy Surf device access installed. Unplug and reconnect the device.'
