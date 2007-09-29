EXTRA_DIST = \
	     splashy_functions.h \
	     splashy.h \
	     splashy_config-functions.h \
	     xml_functions.h \
	     xml_format.h \
	     xml_format.h.in \
	     xml_parser.h \
	     splashycnf.h \
	     common_macros.h

SPL_SONAME = 1:0:0

INCLUDES = $(all_includes) $(splashy_CFLAGS)

sbin_PROGRAMS = splashy splashy_config splashy_update 
lib_LTLIBRARIES = libsplashycnf.la libsplashy.la
include_HEADERS = splashy.h splashycnf.h

## HACK ALERT! - Tim, Vincent, Otavio
# directfb-config is currently broken in etch. It generates the lines below 
# with incorrect capitalization.
DFBHACK = \
	-Wl,-uIDirectFBFont_Default \
	-Wl,-uIDirectFBFont_FT2 \
	-Wl,-uIDirectFBImageProvider_PNG 

libsplashy_la_SOURCES = splashy_video.c
libsplashy_la_LIBADD = -lglib-2.0 -lsplashycnf -ldirectfb
# I had to remove '-lgcc_s' there is no static version for it
# You I'll have to make sure it is available by explicitly linking to it.
libsplashy_la_LDFLAGS = -export-symbols-regex splashy_.* \
			-version-info $(SPL_SONAME) 
#libsplashy_la_LDFLAGS = -export-symbols-regex splashy_.* \
#			-version-info $(SPL_SONAME) \
#			-all-static $(shell directfb-config  --libs --input=keyboard --imageprovider=png --font=ft2,default) $(DFBHACK)


libsplashycnf_la_SOURCES = xml_config.c xml_parser.c 
libsplashycnf_la_LIBADD = -lglib-2.0
libsplashycnf_la_LDFLAGS = -export-symbols-regex splashy_.*\
			   -version-info $(SPL_SONAME)

splashy_SOURCES = splashy_main.c splashy_functions.c
splashy_LDADD = libsplashycnf.la libsplashy.la -lpthread -lm  -lglib-2.0
#splashy_LDFLAGS = -static
# Debian:
splashy_LDFLAGS = -all-static $(shell directfb-config  --libs --input=keyboard --imageprovider=png --font=ft2,default) $(DFBHACK)
# Fedora:
#splashy_LDFLAGS = $(shell directfb-config --libs) $(DFBHACK) -rpath $(libdir)/directfb-1.0-0/systems:$(libdir)/directfb-1.0-0/wm:/usr/lib64/directfb-1.0-0/interfaces/IDirectFBFont/:$(libdir)/directfb-1.0-0/interfaces/IDirectFBImageProvider:$(libdir)/directfb-1.0-0/inputdrivers

splashy_config_SOURCES = splashy_config-main.c splashy_config-functions.c \
	xml_functions.c
splashy_config_LDFLAGS = -lglib-2.0 -lmagic -lz -lsplashycnf

splashy_update_SOURCES = splashy_update.c

install-exec-hook:
	cd $(DESTDIR)$(sbindir) && \
        rm -f splashy_chvt && \
        $(LN_S) splashy splashy_chvt
