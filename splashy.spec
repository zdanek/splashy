#
# spec file for package splashy (Version 0.3.7)
#
# Copyright (c) 2007 SUSE LINUX Products GmbH, Nuernberg, Germany.
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#
# Please submit bugfixes or comments to: splashy-devel@lists.alioth.debian.org
#

Summary:    A complete user-space boot splash system 
Name:       splashy
Version:    0.3.7
Release:    1
License:    GPL
Group:      System/Boot
URL:        http://splashy.alioth.debian.org

Source0:     %{name}-%{version}.tar.gz

BuildRoot:  %{_tmppath}/%{name}-%{version}-buildroot

BuildRequires:  directfb-devel
BuildRequires:  glib2-devel
BuildRequires:  libpng-devel
BuildRequires:  libjpeg-devel
BuildRequires:  freetype-devel
BuildRequires:  file-devel

%description
Splashy is a next generation boot splashing system for Linux systems.
Unlike other splashing systems, it needs no patches to the kernel and
it's installed like a normal package. Make your boot process eye-candy
with Splashy!

        Some of Splashy's most noticable features include:

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

        * Image/animation file formats supported: jpg, png, gif,mpg, swf

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
        Please refer to doc/AUTHORS

%package -n	splashy-devel
Summary:        Development tools for programs using libsplashy
Group:          Development/Languages/C and C++
Requires:       splashy = %{version}

%description -n	splashy-devel
This package contains the header files and libraries needed for
developing programs using libsplashy.

%prep

%setup

        find . -type f | xargs perl -pi -e "s|/usr/lib/|%{_libdir}/|g"
        find . -type f | xargs perl -pi -e "s|/lib/|/%{_lib}/|g"

%build

        ./autogen.sh --prefix=/ --sbindir=/sbin --sysconfdir=/etc \
        --includedir=/usr/include --datarootdir=/usr/share \
        --mandir=/usr/share/man --disable-static

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
#Note: splashy runs better (aesthetically) when run from rc.sysinit 
        rm -f .%{_sysconfdir}/init.d/splashy

#rm symlink (do it in post)
        rm -f  .%{_sysconfdir}/splashy/themes

#%find_lang %{name}

%clean
        %{__rm} -rf %{buildroot}

%post -p /sbin/ldconfig
        #make the themes link (see above)
        ln -s /usr/share/splashy/themes/ /etc/splashy/themes 

%postun -p /sbin/ldconfig
        rm -f  etc/splashy/themes

%files
%defattr(-, root, root, 0755)
%doc AUTHORS ChangeLog COPYING INSTALL NEWS README TODO
%doc %{_mandir}/man?/*
%attr(0644,root,root) %config(noreplace) %{_sysconfdir}/default/*
%dir %{_sysconfdir}/splashy
%config(noreplace) %{_sysconfdir}/splashy/*
%_sbindir/*
%dir %{_datadir}/splashy
%{_datadir}/splashy/*
%{_libdir}/lib%{name}*.so.*

#package language files
%{_datadir}/locale/*/*/*.mo

%files -n splashy-devel
%defattr(-,root,root)
%{_includedir}/%{name}*.h
%{_libdir}/lib%{name}*.so

%changelog
* Fri Nov 28 2007 Rehan Khan <rehan.khan@dsl.pipex.com>
- Initial import for Fedora (it works but it's not perfect)
* Fri Sep 28 2007 Luis Mondesi <lemsx1@gmail.com>
- Copied steps from OpenSUSE's spec file
* Wed Jan 04 2006 Luis Mondesi <lemx1@gmail.com>
- Initial package. (using DAR)
- Modified _template.spec for splashy
