MAINTAINERCLEANFILES = Makefile.in

filesdir = $(datadir)/mokosuite/phone/themes
files_DATA = gry.edj

EDC_DEPS = undo.png

EXTRA_DIST = $(files_DATA) $(EDC_DEPS) theme.edc

clean-local:
	rm -f $(files_DATA)

gry.edj: theme.edc $(EDC_DEPS)
	@edje_cc@ $< $@
