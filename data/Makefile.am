SUBDIRS = themes

MAINTAINERCLEANFILES = Makefile.in

filesdir = $(datadir)/mokosuite/phone
files_DATA = call-end.png call-hold.png call-start.png \
	log_call-in.png log_call-missed.png log_call-out.png

desktopdir = $(datadir)/applications
desktop_DATA = mokophone-phone.desktop

bin_SCRIPTS = mokophone-activate.sh

EXTRA_DIST = $(files_DATA) $(desktop_DATA)
