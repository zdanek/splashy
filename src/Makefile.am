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
# adding -ldirectfb here breaks the build process. Luis
libsplashy_la_LIBADD = -lglib-2.0 -lsplashycnf -ldirectfb
libsplashy_la_LDFLAGS = -export-symbols-regex splashy_.* \
			-version-info $(SPL_SONAME) 

libsplashycnf_la_SOURCES = xml_config.c xml_parser.c 
libsplashycnf_la_LIBADD = -lglib-2.0
libsplashycnf_la_LDFLAGS = -export-symbols-regex splashy_.*\
			   -version-info $(SPL_SONAME)

# '-lgcc_s' is provided with -static-libgcc
splashy_SOURCES = splashy_main.c splashy_functions.c
splashy_LDADD = libsplashycnf.la libsplashy.la -lpthread -lm  -lglib-2.0 @DFB_LIBS@
# Debian:
splashy_LDFLAGS = -all-static -static-libgcc $(DFBHACK)
# Fedora:
#splashy_LDFLAGS = $(shell directfb-config --libs) $(DFBHACK) -rpath $(libdir)/directfb-1.0-0/systems:$(libdir)/directfb-1.0-0/wm:/usr/lib64/directfb-1.0-0/interfaces/IDirectFBFont/:$(libdir)/directfb-1.0-0/interfaces/IDirectFBImageProvider:$(libdir)/directfb-1.0-0/inputdrivers

splashy_config_SOURCES = splashy_config-main.c splashy_config-functions.c \
	xml_functions.c
splashy_config_LDFLAGS = -lsplashycnf -lglib-2.0 -lmagic -lz

splashy_update_LDFLAGS = -all-static -static-libgcc
splashy_update_SOURCES = splashy_update.c

install-exec-hook:
	cd $(DESTDIR)$(sbindir) && \
        rm -f splashy_chvt && \
        $(LN_S) splashy splashy_chvt
