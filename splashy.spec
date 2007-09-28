#
# spec file for package splashy (Version 0.3.3)
#
# Copyright (c) 2007 SUSE LINUX Products GmbH, Nuernberg, Germany.
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#
# Please submit bugfixes or comments to: splashy-devel@lists.alioth.debian.org
#

Summary:    A complete user-space boot splash system 
Name:       splashy
Version:    0.3.6 
Release:    1
License:    GPL
Group:      System/Boot
URL:        http://splashy.alioth.debian.org

Source:     %{name}_%{version}.tar.gz
BuildRoot:  %{_tmppath}/%{name}-%{version}-%{release}-build

#Buildarch:      noarch
BuildRequires:  DirectFB-devel file-devel
BuildRequires:  glib2-devel
#BuildRequires:  ImageMagick-devel
#BuildRequires:	procps-devel
BuildRequires:  libpng-devel
BuildRequires:  libjpeg-devel
BuildRequires:  freetype2-devel
#Requires:

%description
Splashy is a next generation boot splashing system for Linux systems.
Unlike other splashing systems, it needs no patches to the kernel and
its installed like a normal package. Make your boot process eye-candy
with Splashy!

Some of Splashys most noticable features include:

* Require zero kernel patches/full functionality in user-space

* Boot/halt/reboot/runlevel-switch support

* Progressbar support (with optional border)

* Verbose mode (with F2/ESC keys)

* Configuration file in XML

* Cope with any video-mode resolution/size

* Cope with 8, 16, and 24 bit framebuffers

* Alpha channel (transparency) support

* Video mode detection

* Initramfs support

* TrueType2 fonts support

* Lots of image/animation file formats supported: jpg, png, gif,
   mpg, swf

* Low dependencies and code in C to best perform

* Full LSB support

* Multiple themes support

* Really easy to create new themes

* X detection on exit

* Smooth progressbar movement

* Animations support

* Fade in/out effects

* Totally configurable

Authors:
--------
    Jacobo Vilella <jacobo221@gmail.com>
    Luis Mondesi <lemsx1@gmail.com>
    Otavio Salvador <otavio@debian.org>
    Tim Dijkstra <newsuser@famdijkstra.org>
    Vicenzo Ampolo <Vicenzo.Ampolo@gmail.com>
    Vincent Amouret <vincent.amouret@gmail.com>
    Andrew Williams <mistik1@geeksinthehood.net>
    Goedson Teixeira Paixao
    Mario Izquierdo
    Pat Suwalski <pats@xandros.com>
    Alban Browaeys <prahal@yahoo.com>

%debug_package
%package -n	splashy-devel
Summary:        Development tools for programs using libsplashy
Group:          Development/Languages/C and C++
Requires:       splashy = %{version}

%description -n	splashy-devel
This package contains the header files and libraries needed for
developing programs using libsplashy.


Authors:
--------
    Jacobo Vilella <jacobo221@gmail.com>
    Luis Mondesi <lemsx1@gmail.com>
    Otavio Salvador <otavio@debian.org>
    Tim Dijkstra <newsuser@famdijkstra.org>
    Vicenzo Ampolo <Vicenzo.Ampolo@gmail.com>
    Vincent Amouret <vincent.amouret@gmail.com>
    Andrew Williams <mistik1@geeksinthehood.net>
    Goedson Teixeira Paixao
    Mario Izquierdo
    Pat Suwalski <pats@xandros.com>
    Alban Browaeys <prahal@yahoo.com>

%prep
%setup -q
#%patch0 -p1 -b .noWerror
find . -type f | xargs perl -pi -e "s|/usr/lib/|%{_libdir}/|g"
find . -type f | xargs perl -pi -e "s|/lib/|/%{_lib}/|g"

%build
NOCONFIGURE=1 ./autogen.sh
%configure --prefix=/ --disable-static
%{__make} %{?_smp_mflags}

%install
export GCONF_DISABLE_MAKEFILE_SCHEMA_INSTALL="1"
%makeinstall
rm %{buildroot}/usr/lib*/libsplashy*.la
# remove Debian-specific files
cd %{buildroot}
rm -f  .%{_sysconfdir}/lsb-base-logging.sh
rm -rf .%{_sysconfdir}/console-tools
rm -rf .%{_datadir}/initramfs-tools
#remove splashy init script for now, it's not working anyway
#TODO, fixup init-script, install with chmod +x, link to /usr/sbin/rcsplash
rm -f .%{_sysconfdir}/init.d/splashy

#%find_lang %{name}

%clean
%{__rm} -rf %{buildroot}

%post
%run_ldconfig

%postun
%run_ldconfig

%files
%defattr(-, root, root, 0755)
%doc AUTHORS ChangeLog COPYING INSTALL LICENSE NEWS README TODO
%doc %{_mandir}/man?/*
%attr(0644,root,root) %config(noreplace) %{_sysconfdir}/default/*
%dir %{_sysconfdir}/splashy
%config(noreplace) %{_sysconfdir}/splashy/*

%_sbindir/*
%dir %{_datadir}/splashy
%{_datadir}/splashy/*
%{_mandir}/man?/*
%{_libdir}/lib%{name}*.so.*

%files -n splashy-devel
%defattr(-,root,root)
%{_includedir}/%{name}*.h
%{_libdir}/lib%{name}*.so

%changelog
* Sun Jan 19 2005 Dag Wieers <dag@wieers.com> - 
- Initial package. (using DAR)
* Mon Jan 04 2006 Luis Mondesi <lemx1@gmail.com> -
- Modified _template.spec for splashy
* Fri Sep 28 2007 Luis Mondesi <lemsx1@gmail.com> -
- Copied steps from OpenSUSE's spec file
