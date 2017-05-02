Build and install into `/usr/local`
-----------------------------------

    cd ne-x.y.z
    make build install

Note that you must have adequate privileges.


Build and install into some alternative location
------------------------------------------------

Set the PREFIX variable, for example:

    cd ne-x.y.z
    make PREFIX=/home/<you>/opt build install


Makefiles
---------

There are three makefiles provided with the distribution: a top-level
makefile for easy build and installation, a low-level makefile in the
src directory that just builds ne's executable and provides some more
flexibility, and another low-level makefile in the doc directory.

The top-level makefile provides targets "source" (builds the standard
source distribution), "cygwin" (builds the cygwin distribution), "build"
(builds ne) and "install" (installs ne). The PREFIX make variable (see
above) decides where ne will be installed and which will be its global
directory. The LIBS variable can be used to change the name of the curses
library: by default, the low-level makefile uses "-lcurses", but depending
on your system you might prefer to specify "LIBS=-lncurses",
"LIBS=-lncursesw" or even "LIBS=-lterminfo".

For installation (i.e., "make install"), a POSIX compliant machine with a
terminfo database should be sufficient. Note that terminfo might come
bundled in a package named "curses", "ncurses" or some variant of it, and
you may need to install the ncurses development files. Choose the simplest
variant of this package, as ne does not actually use curses (a virtual
screen library), but just the underlying terminfo database.

You may receive some errors from the install-info command if you do not
have write access to the system infodir.bak file; these can be ignored.
Note, however, that creating a source distribution with the "source"
target requires a complete build, and in particular the presence of a
number of tools that manipulate texinfo files, as some of the source files
are generated from the documentation.

By playing with the low-level src makefile you have more options (as you can
first build using the low-level makefile and then use the install target
of the top-level makefile). If you have a termcap database, you should
specify "NE_TERMCAP=1" (i.e., type "make NE_TERMCAP=1"). It uses the GNU
version of termcap, whose sources are included (no library is needed). In
general, if a compilation via a simple "make" fails you should try these
variations in order until one of them succeeds:

    make
    make NE_POSIX=1
    make NE_TERMCAP=1
    make NE_POSIX=1 NE_TERMCAP=1

They use slightly different #define's to overcome the slight differences
among systems. If you have a problem with the local compiler and you have the
GNU C compiler installed, try "CC=gcc", and possibly also "OPTS=-ansi".

If you are compiling under Cygwin or similar emulations of UN*X running
under other operating systems, you can specify "NE_ANSI=1" to build a copy
of ne that by default will use built-in ANSI terminal control sequences.
By combining "NE_ANSI=1" and "NE_TERMCAP=1" you will get a version of ne
that needs no library, and moreover starts by default in ANSI mode.
Regardless of how ne was built, you can always override this choice by
invoking ne with one of the command line options "--ansi" or "--no-ansi".

ne can handle UTF-8, and supports multiple-column characters. The latter
requires some support from the system: you can disable wide-character,
multiple-column support with "NE_NOWCHAR=1".

If you cannot install ne as root, you can change the position of the
global preferences directory with "NE_GLOBAL_DIR=directory" (this is
done automatically by the top-level makefile on the basis of the PREFIX
variable). The global directory should contain automatic preferences files
for common extensions, and must contain the syntax directory provided with
ne's distribution, which contains joe's syntax definition files. In any
case, if the NE_GLOBAL_DIR environment variable is set ne will use its
value instead. The value ne ultimately uses, whether compiled in
or from the environment, is displayed at startup if no file is open.

Compatibility problems are also discussed in the documentation. Don't be
alarmed if you get a lot of warnings about signed vs. unsigned values.
If something does not work, please feel free to email us.


* seba (<mailto:sebastiano.vigna@unimi.it>)
* Todd (<mailto:Todd_Lewis@unc.edu>)
* <mailto:niceeditor@googlegroups.com>
