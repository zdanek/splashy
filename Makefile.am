SUBDIRS = po doc src scripts

pkgsysconfdir=$(sysconfdir)/@PACKAGE@

# This will canonically be installed in /usr/share/splashy
nobase_pkgdata_DATA = \
	themes/default/background.png \
	themes/default/error.png \
	themes/default/suspend.png \
	themes/default/theme.xml \
	themes/default/FreeSans.ttf \
	schemas/theme.xsd \
	schemas/theme.xsl \
	schemas/config.xsd
	
install-exec-hook:
	cd $(DESTDIR)$(pkgsysconfdir) && \
        $(LN_S) $(pkgdatadir)/themes themes

EXTRA_DIST = config.rpath m4/ChangeLog  \
	$(nobase_pkgdata_DATA)  \
	autogen.sh  \
	splashy.spec \
	HACKING 

ACLOCAL_AMFLAGS = -I m4

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = splashy.pc
