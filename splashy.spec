# $Id$
# Authority: dag
# Upstream: 

Summary: A complete user-space boot splash system 
Name: splashy
Version: 0.1.6 
Release: 1
License: GPL
Group: 
URL: http://alioth.debian.org/download.php/1358/splashy_%{version}.tar.gz

Source: %{name}_%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

Buildarch: noarch
BuildRequires: directfb-devel-0.9.22, glib2-devel
Requires:

%description
A boot splash program that doesn't require patching the Linux kernel.
It paints graphic images directly to framebuffers using libdirectfb.

# %package devel
# Summary: Header files, libraries and development documentation for %{name}.
# Group: Development/Libraries
# Requires: %{name} = %{version}-%{release}

# %description devel
# This package contains the header files, static libraries and development
# documentation for %{name}. If you like to develop programs using %{name},
# you will need to install %{name}-devel.

%prep
%setup

# %{__cat} <<EOF >%{name}.desktop
# [Desktop Entry]
# Name=Name Thingy Tool
# Comment=Do things with things
# Icon=name.png
# Exec=name
# Terminal=false
# Type=Application
# StartupNotify=true
# Categories=GNOME;Application;AudioVideo;
# EOF

%build
# %{__libtoolize} --force --copy
# %{__aclocal} #--force
# %{__autoheader}
# %{__automake} --add-missing -a --foreign
# %{__autoconf}
# autoreconf --force --install --symlink
./autogen.sh --prefix=/
#%configure --prefix=/
%{__make} %{?_smp_mflags}

%install
%{__rm} -rf %{buildroot}
export GCONF_DISABLE_MAKEFILE_SCHEMA_INSTALL="1"
%makeinstall
%find_lang %{name}

# %{__install} -d -m0755 %{buildroot}%{_datadir}/applications/
# desktop-file-install --vendor net                  \
# 	--add-category X-Red-Hat-Base              \
# 	--dir %{buildroot}%{_datadir}/applications \
# 	%{name}.desktop

%post
#/sbin/ldconfig 2>/dev/null
#export GCONF_CONFIG_SOURCE="$(gconftool-2 --get-default-source)"
#gconftool-2 --makefile-install-rule %{_sysconfdir}/gconf/schemas/%{name}.schemas &>/dev/null

%postun
#/sbin/ldconfig 2>/dev/null

%clean
%{__rm} -rf %{buildroot}

%files -f %{name}.lang
%defattr(-, root, root, 0755)
%doc AUTHORS ChangeLog COPYING CREDITS FAQ INSTALL LICENSE NEWS README THANKS TODO
%doc %{_mandir}/man?/*
%{_bindir}/*
#%{_libdir}/*.so.*
%{_datadir}/pixmaps/*.png
#%{_datadir}/applications/*.desktop

# %files devel
# %{_includedir}/*.h
# %{_libdir}/*.a
# %{_libdir}/*.so
# %exclude %{_libdir}/*.la

%changelog
* Son Jan 19 2005 Dag Wieers <dag@wieers.com> - 
- Initial package. (using DAR)
* 2006-01-04 10:59 EST Luis Mondesi <lemx1@gmail.com> -
- Modified _template.spec for splashy
