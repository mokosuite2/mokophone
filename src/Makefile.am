VALAC_ARGS = \
	--vapidir=$(top_srcdir)/vapi \
	--pkg dbus-glib-1 \
	--pkg phone \
	--save-temps \
	--basedir $(top_srcdir)/src

INCLUDES = $(GLIB_CFLAGS) $(DBUS_CFLAGS) $(ELEMENTARY_CFLAGS) $(FSO_CFLAGS) \
	$(PHONEUTILS_CFLAGS) $(MOKOSUITE_CFLAGS) $(ALSA_CFLAGS) $(NOTIFY_CFLAGS) \
	-DLOCALEDIR=\""$(localedir)"\" -DDATADIR=\""$(datadir)"\" -DSYSCONFDIR=\""$(sysconfdir)"\" -Wall

bin_PROGRAMS = mokophone

BUILT_SOURCES = .vala.stamp

.vala.stamp: $(mokophone_VALASOURCES)
	$(VALAC) -C $(VALAC_ARGS) -H $(^:.vala=.h) $^
	sed -i 's/\#include <$(^:.vala=.h)>//' $(^:.vala=.c)
	touch $@

mokophone_VALASOURCES = \
	mokophone-service.vala

mokophone_SOURCES = \
	$(mokophone_VALASOURCES:.vala=.c) \
	$(mokophone_VALASOURCES:.vala=.h) \
	main.c \
	globals.h \
	gsm.c \
	gsm.h \
	simauthwin.c \
	simauthwin.h \
	phonewin.c \
	phonewin.h \
	callwin.c \
	callwin.h \
	callblock.c \
	callblock.h \
	logview.c \
	logview.h \
	logentry.c \
	logentry.h \
	contactsview.c \
	contactsview.h \
	sound.c \
	sound.h

mokophone_LDADD = \
	$(GLIB_LIBS) $(DBUS_LIBS) $(ELEMENTARY_LIBS) \
	$(FSO_LIBS) $(PHONEUTILS_LIBS) $(MOKOSUITE_LIBS) \
	$(ALSA_LIBS) $(NOTIFY_LIBS)

EXTRA_DIST = \
	$(BUILT_SOURCES) \
	$(mokophone_VALASOURCES:.vala=.c) \
	$(mokophone_VALASOURCES:.vala=.h)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(mokophone_VALASOURCES:.vala=.c) \
	$(mokophone_VALASOURCES:.vala=.h)

MAINTAINERCLEANFILES = \
	Makefile.in
