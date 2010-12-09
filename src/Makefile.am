INCLUDES = $(GLIB_CFLAGS) $(DBUS_CFLAGS) $(ELEMENTARY_CFLAGS) $(FSO_CFLAGS) $(PHONEUTILS_CFLAGS) $(MOKOSUITE_CFLAGS) \
	-DLOCALEDIR=\""$(localedir)"\" -DDATADIR=\""$(datadir)"\" -DSYSCONFDIR=\""$(sysconfdir)"\" -Wall

bin_PROGRAMS = mokophone

mokophone_SOURCES = \
	main.c \
	globals.h \
	gsm.c \
	gsm.h \
	simauthwin.c \
	simauthwin.h

mokophone_LDADD = \
	$(GLIB_LIBS) $(DBUS_LIBS) $(ELEMENTARY_LIBS) \
	$(FSO_LIBS) $(PHONEUTILS_LIBS) $(MOKOSUITE_LIBS)

MAINTAINERCLEANFILES = \
	Makefile.in
