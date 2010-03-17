Summary: ne, the nice editor
Name: ne
Version: 2.1
Release: 1
License: GPL
Group: Applications/Editors
Source: http://ne.dsi.unimi.it/ne-%{version}.tar.gz
Buildroot: /tmp/ne-%{version}
AutoReqProv: no
Prereq: /sbin/install-info
Requires: ncurses
BuildRequires: ncurses-devel, make, bash, perl, texinfo, sed

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
mkdir -p $RPM_BUILD_ROOT/%{_infodir}
mkdir -p $RPM_BUILD_ROOT/%{_mandir}/man1
mkdir -p $RPM_BUILD_ROOT/%{_docdir}/ne-%{version}/html
install -m 755 ./src/ne $RPM_BUILD_ROOT/usr/bin/ne
install -m 644 ./syntax/*.jsf $RPM_BUILD_ROOT/usr/share/ne/syntax
install -m 644 ./doc/ne.1 $RPM_BUILD_ROOT/%{_mandir}/man1
install -m 644 ./doc/ne.info* $RPM_BUILD_ROOT/%{_infodir}
mv ./doc/ne html

%files
%defattr(-,root,root)
/usr/bin/ne
/usr/share/ne/syntax/*.jsf
%{_mandir}/man1/ne.1*
%{_infodir}/ne.info*
%doc html
%doc ./doc/ne.texinfo
%doc ./doc/ne.pdf
%doc ./doc/ne.txt
%doc ./doc/default.*
%doc ./README
%doc ./CHANGES
%doc ./COPYING

%post
/sbin/install-info %{_infodir}/ne.info.gz %{_infodir}/dir

%preun
if [ "$1" = 0 ]; then
    /sbin/install-info --delete %{_infodir}/ne.info.gz %{_infodir}/dir
fi


%changelog
* Wed Mar 17 2010 Sebastiano Vigna <vigna@dsi.unimi.it> 2.1
- Upgraded to 2.1.
* Fri Feb 20 2009 Sebastiano Vigna <vigna@dsi.unimi.it> 2.0.3
- Upgraded to 2.0.3.
* Wed Feb 18 2009 Sebastiano Vigna <vigna@dsi.unimi.it> 2.0.2
- Upgraded to 2.0.2.
* Tue Feb 17 2009 Sebastiano Vigna <vigna@dsi.unimi.it> 2.0.1
- Upgraded to 2.0.1.
* Mon Feb 16 2009 Sebastiano Vigna <vigna@dsi.unimi.it> 2.0
- First major new release. We got syntax highlighting.
* Wed Apr 2 2008 Sebastiano Vigna <vigna@dsi.unimi.it> 1.43
- Fixed paragraphing, menu search, and, finally, the old
  BSD escape bug.
* Sun Jan 22 2006 Sebastiano Vigna <vigna@dsi.unimi.it> 1.42
- Fixed wrong file order in file requester.
* Thu Sep 1 2005 Sebastiano Vigna <vigna@dsi.unimi.it> 1.41
- Now find does not skip the first entry.
* Thu Jul 21 2005 Sebastiano Vigna <vigna@dsi.unimi.it> 1.40
- Fixed bug in handling the encoding of the history buffer.
* Tue Nov 23 2004 Sebastiano Vigna <vigna@dsi.unimi.it> 1.39
- Added built-in sequences for Home/End key in gnome-terminal
* Mon Sep 27 2004 Sebastiano Vigna <vigna@dsi.unimi.it> 1.38
- Fixed include
* Fri Sep 24 2004 Sebastiano Vigna <vigna@dsi.unimi.it> 1.37
- Better bookmarks and binary clips.
* Tue Aug 17 2004 Sebastiano Vigna <vigna@dsi.unimi.it> 1.36
- Fixed minor bug in paste on free form documents.
* Tue Jul 6 2004 Sebastiano Vigna <vigna@dsi.unimi.it> 1.35
- We no longer reset the terminal. Fixed UTF-8 sequence parsing.
* Sat Jun 26 2004 Sebastiano Vigna <vigna@dsi.unimi.it> 1.34
- 8-bit local-dependent casing.
* Sat May 1 2004 Sebastiano Vigna <vigna@dsi.unimi.it> 1.33
- Fixed screen update problem.
* Sat Apr 17 2004 Sebastiano Vigna <vigna@dsi.unimi.it> 1.32
- Fixed UTF-8 whitespace/punctuation and screen update problem.
* Mon Apr 05 2004 Sebastiano Vigna <vigna@dsi.unimi.it> 1.31
- Fixed problem with makefile variables.
* Fri Apr 02 2004 Sebastiano Vigna <vigna@dsi.unimi.it> 1.30
- Major rewriting of all components to support UTF-8.
* Sat Oct 28 2000 Sebastiano Vigna <vigna@dsi.unimi.it>
- New release, with a few bug fixes
* Fri Mar 31 2000 Sebastiano Vigna <vigna@dsi.unimi.it>
- Special build for systems with old ncurses
* Tue Jun 1 1999 Sebastiano Vigna <vigna@dsi.unimi.it>
- Final distribution for general consumption
* Mon May 31 1999 Sebastiano Vigna <vigna@dsi.unimi.it>
- Info pages are now gzip'd and go to /usr/info
* Thu May 27 1999 Sebastiano Vigna <vigna@dsi.unimi.it>
- Added man page, made turbo parameter global
* Wed May 26 1999 Sebastiano Vigna <vigna@dsi.unimi.it>
- Minor modifications to the documentation
* Tue May 25 1999 Sebastiano Vigna <vigna@dsi.unimi.it>
- Second ne RPM for version 1.17; now requires glibc
* Thu Oct 29 1998 Sebastiano Vigna <vigna@dsi.unimi.it>
- First ne RPM
