Summary: ne, the nice editor
Name: ne
Version: 3.3.3
Release: 1%{?dist}
License: GPLv3+
Source0: https://ne.di.unimi.it/ne-%{version}.tar.gz
URL: https://ne.di.unimi.it/
Requires: ncurses
BuildRequires: gcc
BuildRequires: ncurses-devel
BuildRequires: make
BuildRequires: bash
BuildRequires: perl
BuildRequires: texinfo
BuildRequires: sed

%description 
ne is a free (GPL'd) text editor based on the POSIX standard that runs (we
hope) on almost every UN*X machine. ne is easy to use for the beginner, but
powerful and fully configurable for the wizard, and most sparing in its
resource usage.

%prep
%setup -q

%build
cd src
%make_build NE_GLOBAL_DIR=%{_datadir}/ne LIBS=-lncurses OPTS="%{optflags} -fno-strict-aliasing -Wno-parentheses"

%install
mkdir -p $RPM_BUILD_ROOT%{_bindir}
mkdir -p $RPM_BUILD_ROOT%{_datadir}/ne/syntax
mkdir -p $RPM_BUILD_ROOT%{_datadir}/ne/macros
mkdir -p $RPM_BUILD_ROOT%{_infodir}
mkdir -p $RPM_BUILD_ROOT%{_mandir}/man1
install -p -m 755 ./src/ne $RPM_BUILD_ROOT%{_bindir}/ne
install -p -m 644 ./extensions $RPM_BUILD_ROOT%{_datadir}/ne/extensions
install -p -m 644 ./syntax/*.jsf $RPM_BUILD_ROOT%{_datadir}/ne/syntax
install -p -m 644 ./macros/* $RPM_BUILD_ROOT%{_datadir}/ne/macros
install -p -m 644 ./doc/ne.1 $RPM_BUILD_ROOT%{_mandir}/man1
install -p -m 644 ./doc/ne.info* $RPM_BUILD_ROOT%{_infodir}
rm INSTALL.md
mv doc/html .

%files
%{_bindir}/ne
%{_datadir}/ne/
%{_mandir}/man1/ne.1*
%doc %{_infodir}/ne.info*
%doc ./README.md
%doc ./NEWS
%doc ./CHANGES

%package doc
Summary: Documentation for ne, the nice editor
BuildArch: noarch

%description doc
Documentation for ne, the nice editor.

%files doc
%license ./COPYING
%doc html
%doc ./doc/ne.texinfo
%doc ./doc/ne.pdf
%doc ./doc/ne.txt
%doc ./doc/default.*

%changelog
* Tue Sep 13 2022 Sebastiano Vigna <sebastiano.vigna@gmail.com> - 3.3.2-1
- First release
* Tue May 18 2021 Sebastiano Vigna <sebastiano.vigna@gmail.com> - 3.3.1-1
- First release
