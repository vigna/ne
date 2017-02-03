Summary: ne, the nice editor
Name: ne
Version: 3.0.1
Release: 1
License: GPLv3
Group: Applications/Editors
Source: http://ne.di.unimi.it/ne-%{version}.tar.gz
Buildroot: /tmp/ne-%{version}
AutoReqProv: no
Requires: /sbin/install-info
Requires: ncurses
BuildRequires: ncurses-devel, make, bash, perl, texinfo, sed

%global debug_package %{nil}

%description 
ne is a free (GPL'd) text editor based on the POSIX standard that runs (we
hope) on almost any UN*X machine. ne is easy to use for the beginner, but
powerful and fully configurable for the wizard, and most sparing in its
resource usage. If you have the resources and the patience to use emacs or the
right mental twist to use vi then probably ne is not for you. However, being
fast, small, powerful and simple to use, ne is ideal for email, editing through
phone line (or slow GSM/GPRS) connections and so on. Moreover, the internal
text representation is very compact--you can easily load and modify very large
files.

%prep

%setup -q

%build

cd src; make NE_GLOBAL_DIR=/usr/share/ne; strip ne

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin
mkdir -p $RPM_BUILD_ROOT/usr/share/ne/syntax
mkdir -p $RPM_BUILD_ROOT/usr/share/ne/macros
mkdir -p $RPM_BUILD_ROOT/%{_infodir}
mkdir -p $RPM_BUILD_ROOT/%{_mandir}/man1
mkdir -p $RPM_BUILD_ROOT/%{_docdir}/ne-%{version}/html
install -m 755 ./src/ne $RPM_BUILD_ROOT/usr/bin/ne
install -m 644 ./syntax/*.jsf $RPM_BUILD_ROOT/usr/share/ne/syntax
install -m 644 ./macros/* $RPM_BUILD_ROOT/usr/share/ne/macros
install -m 644 ./doc/ne.1 $RPM_BUILD_ROOT/%{_mandir}/man1
install -m 644 ./doc/ne.info* $RPM_BUILD_ROOT/%{_infodir}
rm INSTALL
mv doc/html .

%files
%defattr(-,root,root)
/usr/bin/ne
/usr/share/ne/syntax/*.jsf
/usr/share/ne/macros/*
%{_mandir}/man1/ne.1*
%{_infodir}/ne.info*
%doc html
%doc ./doc/ne.texinfo
%doc ./doc/ne.pdf
%doc ./doc/ne.txt
%doc ./doc/default.*
%doc ./README.md
%doc ./NEWS
%doc ./CHANGES
%doc ./COPYING

%post
/sbin/install-info %{_infodir}/ne.info.gz %{_infodir}/dir

%preun
if [ "$1" = 0 ]; then
    /sbin/install-info --delete %{_infodir}/ne.info.gz %{_infodir}/dir
fi
