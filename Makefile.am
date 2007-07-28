SUBDIRS = doc src scripts

pkgsysconfdir=$(sysconfdir)/@PACKAGE@

# This will canonically be installed in /etc/splashy
#   I (TPTD) don't like having the default theme in /etc...
#   If initramfs is working we should just copy the default theme
#   to it and put it in $(pkgdata)
nobase_pkgsysconf_DATA = \
	themes/default/background.png \
	themes/default/error.png \
	themes/default/suspend.png \
	themes/default/theme.xml \
	themes/default/FreeSans.ttf 


# This will canonically be installed in /usr/share/splashy
nobase_pkgdata_DATA = \
	schemas/theme.xsd \
	schemas/theme.xsl \
	schemas/config.xsd
	
EXTRA_DIST = \
	$(nobase_pkgsysconf_DATA) \
	$(nobase_pkgdata_DATA)  \
	autogen.sh  \
	splashy.spec \
	HACKING 

