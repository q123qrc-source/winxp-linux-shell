# SPDX-License-Identifier: GPL-3.0-or-later
# Kickstart outline for the Meadow test VM (completed in phase 11).
text
lang en_US.UTF-8
keyboard us
timezone UTC
selinux --enforcing
rootpw --lock
user --name=alice --password=meadow-test --plaintext --groups=wheel
user --name=bartholomew-maximilian-longname --password=meadow-test --plaintext
bootloader --timeout=1
autopart
reboot

%packages
@core
sddm
labwc
fprintd
fprintd-pam
pamtester
%end
