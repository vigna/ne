Summary: ne, the nice editor
Name: ne
Version: 3.3.0
Release: 1%{?dist}
License: GPLv3+
Source0: http://ne.di.unimi.it/ne-%{version}.tar.gz
URL: http://ne.di.unimi.it/
Buildroot: /tmp/ne-%{version}
AutoReqProv: no
Requires: info
Requires: ncurses
BuildRequires: ncurses-devel, make, bash, perl, texinfo, sed

%global debug_package %{nil}

%description 
ne is a free (GPL'd) text editor based on the POSIX standard that runs (we
hope) on almost any UN*X machine. ne is easy to use for the beginner, but
powerful and fully configurable for the wizard, and most sparing in its
resource usage.

%prep
%setup -q

%build

cd src; make NE_GLOBAL_DIR=%{_datadir}/ne LIBS=-lncurses

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin
mkdir -p $RPM_BUILD_ROOT%{_datadir}/ne/syntax
mkdir -p $RPM_BUILD_ROOT%{_datadir}/ne/macros
mkdir -p $RPM_BUILD_ROOT%{_infodir}
mkdir -p $RPM_BUILD_ROOT%{_mandir}/man1
mkdir -p $RPM_BUILD_ROOT%{_docdir}/ne-%{version}/html
install -m 755 ./src/ne $RPM_BUILD_ROOT/usr/bin/ne
install -m 644 ./extensions $RPM_BUILD_ROOT%{_datadir}/ne/extensions
install -m 644 ./syntax/*.jsf $RPM_BUILD_ROOT%{_datadir}/ne/syntax
install -m 644 ./macros/* $RPM_BUILD_ROOT%{_datadir}/ne/macros
install -m 644 ./doc/ne.1 $RPM_BUILD_ROOT%{_mandir}/man1
install -m 644 ./doc/ne.info* $RPM_BUILD_ROOT%{_infodir}
rm INSTALL.md
mv doc/html .

%files
%defattr(-,root,root)
/usr/bin/ne
%{_datadir}/ne/extensions
%{_datadir}/ne/syntax/*.jsf
%{_datadir}/ne/macros/*
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

%changelog
* Fri Apr 23 2021 Sebastiano Vigna <sebastiano.vigna@gmail.com> - 3.3.1-1
- First release