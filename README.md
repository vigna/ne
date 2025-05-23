# Introduction

`ne` is a free (GPL'd) text editor based on the POSIX standard that runs
(we hope) on almost every UN*X machine. `ne` is easy to use for the
beginner, but powerful and fully configurable for the wizard, and most
sparing in its resource usage. If you have the resources and the patience
to use `emacs` or the right mental twist to use `vi` then probably `ne` is
not for you. However, if you need an editor that:

* compiles without effort *everywhere* (or almost everywhere), is packaged
for all Linux distributions, and ported to other operating systems (such
as Mac OS X);

* is fast, small, powerful and simple to use;

* has standard keystrokes (e.g., copy is CTRL-C);

* uses little bandwidth, so it is ideal for email, editing through phone
line (or slow GSM/GPRS/UMTS) connections;

* has a very compact internal text representation, so you can easily load
and modify very large files…

… then you should try `ne`.

![ne editor screenshot](https://github.com/vigna/ne/ne.png)

Some of the features of `ne` are:

* three user interfaces: control keystrokes, command line, and menus;
keystrokes and menus are completely configurable;

* syntax highlighting;

* full support for UTF-8 files, including multiple-column characters;

* 64-bit file/line length;

* simple scripting language where scripts can be generated *via* an
idiotproof record/play method;

* unlimited undo/redo capability (can be disabled with a command);

* automatic preferences system based on the extension of the file name
being edited or regex content matching;

* automatic completion of prefixes using words in your documents as
dictionary;

* a file requester with completion features for easy file retrieval;

* extended regular expression search and replace a la `emacs` and `vi`;

* a very compact memory model—you can easily load and modify very large
files, even if they do not fit your core memory;

* editing of binary files.

[Linux Voice](http://www.linuxvoice.com/issues/006/foss6.pdf) ranked `ne`
as the third best editor for Linux!

---

# Downloads

There are a few archives related to `ne`: the general distribution is a
tar gzip'd file containing sources that should compile under any
POSIX-compliant UNIX. The RPM archives provide easy installation on
RPM-based distributions.

If you use the [Cygwin environment](http://www.cygwin.com/) there is a
version of `ne` compiled for it (you just have to untar it in the root
directory). This version requires the last version of `ncurses`, but you
can also get a `termcap`-based version that has no dependencies and starts
by default using the built-in ANSI sequences.

`ne` was originally developed on the Amiga and inspired by `TurboText`, a
wonderful editor written by Martin Taillefer. Development moved then on
Linux. Todd Lewis joined the development donating code he wrote to add
features required at the University of North Carolina. Daniele Filaretti
helped with syntax highlighting.

---

# Ports

* [Fedora](https://src.fedoraproject.org/rpms/ne)
* [Debian package](https://packages.debian.org/stable/editors/ne)
* [Ubuntu package](http://packages.ubuntu.com/ne)
* [OpenBSD port](https://github.com/openbsd/ports/tree/master/editors/ne)
* [Mac OS X (Apple Silicon)](ne-3.3.3-AppleSilicon.dmg)
* [Mac OS X (Intel)](ne-3.3.3-Intel.dmg)
* [MacPorts port](https://ports.macports.org/port/ne/)
* [Homebrew formula](https://github.com/Homebrew/homebrew-core/blob/master/Formula/n/ne.rb)
* [Haiku port](https://depot.haiku-os.org/#!/pkg/ne)
* [Cygwin (x86_64, ncurses)](ne-cygwin-3.3.2-x86_64.tar.gz)
* [Cygwin (x86_64, termcap/ANSI)](ne-cygwin-ansi-3.3.2-x86_64.tar.gz)

---

# Discussion

There is a [discussion group](http://groups.google.com/group/niceeditor)
about `ne`. You can join or [send a
message](mailto:niceeditor@googlegroups.com).
