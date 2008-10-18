pkgsysconfdir=$(sysconfdir)/@PACKAGE@

# This will canonically be installed in /etc
nobase_sysconf_DATA = \
	console-tools/config.d/splashy \
	init.d/splashy \
	default/splashy 

# This will canonically be installed in /etc
sysconf_DATA =\
	lsb-base-logging.sh


# This will canonically be installed in /usr/share
nobase_data_DATA = \
 	initramfs-tools/scripts/local-premount/splashy \
 	initramfs-tools/scripts/local-premount/libsplashy \
	initramfs-tools/scripts/init-bottom/splashy \
	initramfs-tools/hooks/splashy \
	initramfs-tools/hooks/libsplashy \
	initramfs-tools/modules.d/splashy

EXTRA_DIST = \
	$(nobase_sysconf_DATA) $(sysconf_DATA) $(nobase_data_DATA)


