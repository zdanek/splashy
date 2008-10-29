dnl $Revision:  $
dnl Process this file with autoconf to produce a configure script.

AC_INIT([splashy], [0.3.13], [splashy-devel@alioth.debian.org])
AC_CONFIG_SRCDIR(src/splashy_main.c)
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE([splashy], [0.3.13], [splashy-devel@alioth.debian.org])

AC_PREREQ(2.59)
AC_LANG_C
AC_PROG_CC
AC_PROG_INSTALL

#AC_ENABLE_STATIC
# when using STATIC, comment the following line (and uncomment the previous one):
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

AC_PROG_LN_S

# Splashy goes international!
GETTEXT_PACKAGE=splashy
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Package Name for Gettext])
# languages that we support:
ALL_LINGUAS="es sk fr de hu pt" 
AM_GLIB_GNU_GETTEXT([external])
AM_GLIB_DEFINE_LOCALEDIR(LOCALEDIR)

# Check library magic existence and its use
AC_CHECK_HEADER([magic.h], [
    AC_CHECK_LIB(magic, magic_open, [
        WITH_MAGIC_SUBDIR=
        WITH_MAGIC_INCLUDE=
        WITH_MAGIC_LIB="-lmagic"
    ])
])

PKG_CHECK_MODULES(splashy, [glib-2.0, directfb >= 0.9.22])
PKG_CHECK_MODULES(splashy_config, [glib-2.0])

# EXTRA ARGS #
# Add an --enable-debug option to turn on DEBUG_PRINT statements
AH_TEMPLATE(DEBUG,[Define to enable debugging messages])
AC_ARG_ENABLE(debug,AC_HELP_STRING([--enable-debug],[enable debugging messages]),AC_DEFINE(DEBUG))
# END EXTRA ARGS

# All warnings are errors.
# when in --enable-debug mode, we need to enable assert in glib...
if test x$enable_debug = x1; then
    CFLAGS="$CFLAGS -Wall -Werror"
else
    CFLAGS="$CFLAGS -Wall -Werror -DG_DISABLE_ASSERT -DG_DISABLE_CHECKS"
fi

# we used to compile splashy statically
AC_SUBST(splashy_LIBS)
AC_SUBST(splashy_CFLAGS)
# other binaries
AC_SUBST(splashy_config_LIBS)
AC_SUBST(splashy_config_CFLAGS)
AC_SUBST(splashy_update_LIBS)
AC_SUBST(splashy_update_CFLAGS)

#
# We need to get expanded versions of a bunch of things
# stolen mostly from gdm
#

SYSCONFDIR_TMP="$sysconfdir"
EXPANDED_SYSCONFDIR=`eval echo $SYSCONFDIR_TMP | sed s/NONE//`
AC_DEFINE([EXPANDED_SYSCONFDIR], [$EXPANDED_SYSCONFDIR], [our system configuration directory])
AC_SUBST(EXPANDED_SYSCONFDIR)

DFB_VERSION=`eval  directfb-config --version`
AC_DEFINE([DFB_VERSION], [$DFB_VERSION], [directfb version])
AC_SUBST(DFB_VERSION)

DFB_LIBDIR="$(eval pkg-config directfb-internal --variable=moduledir)"
AC_DEFINE([DFB_LIBDIR], [$DFB_LIBDIR], [directfb libdir])
AC_SUBST(DFB_LIBDIR)

# If doing an -all-static, you will need to define which keyboard driver
# imageprovider and font you want compiled-in and you will need
# to modify src/Makefile.am where it says "STATIC"
# sample options to include to directfb-config below (before ' | sed ...'):
#--input=keyboard --imageprovider=png --font=ft2,default 
DFB_LIBS="$(eval directfb-config --libs | sed 's/$/ \\/')"
#AC_DEFINE([DFB_LIBS], [$DFB_LIBS], [directfb libs])
AC_SUBST(DFB_LIBS)

# TODO slurp the doc/*.xml.in or .xml files and create xml_format.h
# EXPANDED_XML_CONFIG_FORMAT="`cat doc/config.xml`"
# AC_DEFINE([EXPANDED_XML_CONFIG_FORMAT], [$EXPANDED_XML_CONFIG_FORMAT], [our xml configuration])
# AC_SUBST(EXPANDED_XML_CONFIG_FORMAT)
# 
# EXPANDED_XML_THEME_FORMAT="`cat doc/theme.xml`"
# AC_DEFINE([EXPANDED_XML_THEME_FORMAT], [$EXPANDED_XML_THEME_FORMAT], [our theme configuration])
# AC_SUBST(EXPANDED_XML_THEME_FORMAT)

AC_OUTPUT( po/Makefile.in
          Makefile 
          src/Makefile 
          src/xml_format.h
          doc/Makefile 
          doc/config.xml
          doc/theme.xml
          scripts/Makefile 
          scripts/initramfs-tools/hooks/libsplashy
          Doxyfile
          splashy.pc
          )

echo
echo $PACKAGE $VERSION
echo
echo Print debugging messages...... : $enable_debug
echo
echo configure complete, now type \'make\'
echo
