#
# This is the makefile for ne. 
#
# The default build relies on terminfo, but you can specify NE_TERMCAP=1 to rely on
# termcap, or even NE_ANSI=1 to rely on termcap and to use by 
# default the built-in ANSI terminal description. In the
# latter two cases, no external library is needed. You can also add the
# definition NE_POSIX=1 if you get errors. Specifying NE_DEBUG=1 will enable
# debugging info and will compile in a number of assertions. Moreover,
# specifying NE_NOWCHAR=1 will remove the calls to wide character versions
# of toupper(), tolower(), isspace(), etc., and also calls to wcwidth().
# Another good idea if you are using gcc is to try OPTS=-ansi.
#
# By changing the value of NE_GLOBAL_DIR, you can change the directory
# where ne tries to read system-wide information (configuration
# files, ,etc.).
#

NE_GLOBAL_DIR     = /usr/local/share/ne

PROGRAM       = ne

OBJS	      = actions.o \
		ansi.o \
		buffer.o \
		clips.o \
		cm.o \
		command.o \
		display.o \
		edit.o \
		errors.o \
		exec.o \
		ext.o \
		hash.o \
		help.o \
		input.o \
		inputclass.o \
		keys.o \
		menu.o \
		names.o \
		navigation.o \
		ne.o \
		prefs.o \
		regex.o \
		request.o \
		search.o \
		signals.o \
		streams.o \
		support.o \
		syn_hash.o \
		syn_regex.o \
		syn_utf8.o \
		syn_utils.o \
		syntax.o \
		term.o \
		undo.o \
		utf8.o
		


TERMCAPOBJS   = tparam.o \
		info2cap.o \
		termcap.o

NE_POSIX=
NE_TERMCAP=
NE_ANSI=
NE_NOWCHAR=
NE_DEBUG=
NE_TEST=

ifeq ($(CC),gcc)
NO_SIGN=-Wno-pointer-sign
endif

CFLAGS=$(OPTS) $(NO_SIGN) $(if $(NE_POSIX),-D_POSIX_C_SOURCE=199506L,) $(if $(NE_NOWCHAR),-DNOWCHAR,) $(if $(NE_TEST),-DNE_TEST,) $(if $(NE_DEBUG),-g,-O3 -DNODEBUG) $(if $(NE_TERMCAP),-DTERMCAP,) $(if $(NE_ANSI),-DTERMCAP -DANSI,)
LIBS=$(if $(NE_TERMCAP)$(NE_ANSI),,-lcurses)

ne:	$(OBJS) $(if $(NE_TERMCAP)$(NE_ANSI),$(TERMCAPOBJS),)
	$(CC) $(OPTS) $(LDFLAGS) $^ $(LIBS) -o $(PROGRAM)

clean:
	rm -f *.o core

really-clean: clean
	rm -f ne hash.h hash.c help.c help.h names.c names.h enums.h ext.c

###

ne.texinfo: ../doc/ne.texinfo
	-ln -s ../doc/ne.texinfo

ne.h: enums.h utf8.h

ext.c: ext.c.in

enums.h names.c names.h hash.c hash.h help.c help.h ext.c: ne.texinfo info2src.pl
	makeinfo -D autohelp ne.texinfo
	perl info2src.pl
	rm -f ne.info*

actions.o: ne.h	keycodes.h names.h errors.h protos.h version.h

buffer.o: ne.h keycodes.h names.h errors.h protos.h

clips.o: ne.h keycodes.h names.h errors.h protos.h

cm.o: cm.h

command.o: ne.h keycodes.h names.h errors.h protos.h help.h hash.h

display.o: ne.h keycodes.h names.h errors.h protos.h termchar.h

edit.o: ne.h keycodes.h names.h errors.h protos.h

errors.o: errors.h

exec.o: ne.h keycodes.h names.h errors.h protos.h

hash.o: hash.h

info2cap.o: info2cap.c info2cap.h

input.o: ne.h keycodes.h names.h errors.h protos.h

inputclass.o: ne.h keycodes.h names.h errors.h protos.h

keys.o: ne.h keycodes.h names.h errors.h protos.h

menu.o: ne.h keycodes.h names.h errors.h protos.h

navigation.o: ne.h keycodes.h names.h errors.h protos.h

ne.o: ne.h keycodes.h names.h errors.h protos.h version.h regex.h

prefs.o: ne.h keycodes.h names.h errors.h protos.h

regex.o: regex.h regex.c
	$(CC) $(CFLAGS) -DREGEX_MALLOC -DSTDC_HEADERS -c regex.c

request.o: ne.h keycodes.h names.h errors.h protos.h

search.o: ne.h keycodes.h names.h errors.h protos.h regex.h

signals.o: ne.h keycodes.h names.h errors.h protos.h

streams.o: ne.h keycodes.h names.h errors.h protos.h

support.o: ne.h keycodes.h names.h errors.h protos.h support.c
	$(CC) $(CFLAGS) -DGLOBALDIR=\"$(NE_GLOBAL_DIR)\" -c support.c

term.o: termchar.h cm.h

termcap.o: termcap.c termcap.h
	$(CC) $(CFLAGS) -DSTDC_HEADERS -c termcap.c

tparam.o: tparam.c termcap.h
	$(CC) $(CFLAGS) -DSTDC_HEADERS -c tparam.c

undo.o: ne.h keycodes.h names.h errors.h protos.h

utf8.o: utf8.h
