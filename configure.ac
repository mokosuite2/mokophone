AC_INIT([MokoPhone], [1.0.99], [daniele.athome@gmail.com], [mokophone])
AC_CONFIG_SRCDIR(Makefile.am)
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE

AC_ISC_POSIX
AC_GNU_SOURCE
AC_PROG_CC
AC_PROG_LIBTOOL
AC_SUBST(ACLOCAL_AMFLAGS, "$ACLOCAL_FLAGS -I m4")

dnl Vala for D-Bus servers
AC_PATH_PROG(VALAC, valac, valac)

dnl Intl for gettext
IT_PROG_INTLTOOL([0.35.0])

if test "x$localedir" == x
then
    localedir='${datadir}/locale'
    AC_SUBST(localedir)
    AM_GLIB_DEFINE_LOCALEDIR([localedir])
fi

AM_GLIB_GNU_GETTEXT
GETTEXT_PACKAGE="mokophone"
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, ["$GETTEXT_PACKAGE"], [Name of the gettext message domain])

dnl Dependencies
ELEMENTARY_REQUIRED=0.0.0
ECORE_REQUIRED=0.0.0
EVAS_REQUIRED=0.0.0
EDJE_REQUIRED=0.0.0
GLIB_REQUIRED=2.18.0
DBUS_REQUIRED=1.1.1
DBUS_GLIB_REQUIRED=0.74
PHONEUTILS_REQUIRED=0.1
FSO_REQUIRED=0.2
MOKOSUITE_REQUIRED=$PACKAGE_VERSION
NOTIFY_REQUIRED=0.4.5

PKG_CHECK_MODULES(GLIB,
  glib-2.0 >= $GLIB_REQUIRED
  gobject-2.0 >= $GLIB_REQUIRED
)

PKG_CHECK_MODULES(DBUS,
  dbus-1 >= $DBUS_REQUIRED
  dbus-glib-1 >= $DBUS_GLIB_REQUIRED
)

PKG_CHECK_MODULES(ELEMENTARY,
  elementary >= $ELEMENTARY_REQUIRED
)

PKG_CHECK_MODULES(ECORE,
  ecore >= $ECORE_REQUIRED
  ecore-x >= $ECORE_REQUIRED
  ecore-evas >= $ECORE_REQUIRED
  ecore-input >= $ECORE_REQUIRED
  eet >= $ECORE_REQUIRED
)
AC_SUBST(ECORE_CFLAGS)
AC_SUBST(ECORE_LIBS)

PKG_CHECK_MODULES(EVAS, evas >= $EVAS_REQUIRED)

PKG_CHECK_MODULES(EDJE, edje >= $EDJE_REQUIRED)

PKG_CHECK_MODULES(FSO, libfreesmartphone-glib >= $FSO_REQUIRED)

PKG_CHECK_MODULES(PHONEUTILS, libphone-utils >= $PHONEUTILS_REQUIRED)

PKG_CHECK_MODULES(MOKOSUITE,
  mokosuite-utils >= $MOKOSUITE_REQUIRED
  mokosuite-ui >= $MOKOSUITE_REQUIRED
  mokosuite-pim >= $MOKOSUITE_REQUIRED
)

PKG_CHECK_MODULES(ALSA, alsa)
AC_SUBST(ALSA_CFLAGS)
AC_SUBST(ALSA_LIBS)

PKG_CHECK_MODULES(NOTIFY, libnotify >= $NOTIFY_REQUIRED)

dnl edje_cc for edj files
AC_ARG_WITH(edje-cc,
[  --with-edje-cc=PATH	specify a specific path to edje_cc],
[
  v=$withval;
  edje_cc=$v
  echo "  Enlightenment edje_cc explicitly set to "$edje_cc;
],[
  edje_cc=$(pkg-config --variable=prefix edje)/bin/edje_cc
])
AC_SUBST(edje_cc)

AC_ARG_ENABLE(debug,
    AC_HELP_STRING([--enable-debug], [Enable debug build (default: disabled)]),
    enable_debug=$enableval,
    enable_debug=no)

if test "x$enable_debug" != "xno"; then
    AC_DEFINE(DEBUG, 1, [Define to enable debug build])
fi

AC_DEFINE(CONTACTSDB_SQLITE, 1, [Enable SQLite contacts database])

AC_OUTPUT([
Makefile
src/Makefile
data/Makefile
data/themes/Makefile
data/themes/gry/Makefile
po/Makefile.in
])
