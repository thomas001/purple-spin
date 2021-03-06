AC_INIT([purple-spin],[1.0],[],[],[http://thomas001.github.com/purple-spin/])
AM_INIT_AUTOMAKE([foreign])

AC_USE_SYSTEM_EXTENSIONS
AC_CONFIG_MACRO_DIR([m4])

AC_DISABLE_STATIC

AC_PREREQ([2.63])
AC_PROG_CC
AC_PROG_LIBTOOL

# AC_USE_SYSTEM_EXTENSIONS

AM_GNU_GETTEXT_VERSION([0.17])
AM_GNU_GETTEXT([external])


# found by autoscan
AC_CHECK_FUNCS([strchr],,
[AC_MSG_ERROR([strchr not found])])

AC_CHECK_HEADERS([string.h],,
[AC_MSG_ERROR([string.h not found])])

AC_CHECK_HEADERS([sys/socket.h],,
[AC_MSG_ERROR([sys/socket.h not found])])

AC_CHECK_HEADERS([unistd.h],,
[AC_MSG_ERROR([unistd.h not found])])

AC_TYPE_SSIZE_T

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES(JSON_GLIB,json-glib-1.0,,[AC_MSG_ERROR([json-glib not found])])

AM_PATH_GLIB_2_0([2.15.0],,[AC_MSG_ERROR([glib not found])])

PKG_CHECK_MODULES(PURPLE,
	[purple >= 2.5.7],
	[PURPLE_PLUGINDIR="`$PKG_CONFIG --variable=plugindir purple`"
	 AS_IF([test "x$PURPLE_PLUGINDIR" = x],
	       [AC_MSG_ERROR([libpurple found, but no plugindir defined])],
	       [AC_MSG_NOTICE([libpurple plugin directory is $PURPLE_PLUGINDIR])
	        AC_SUBST([PURPLE_PLUGINDIR])])],
	[AC_MSG_ERROR([libpurple not found])])

AM_PATH_XML2(,,[AC_MSG_ERROR([libxml2 not found])])

PKG_CHECK_MODULES(PIDGIN,[pidgin >= 2.5.7],
  [have_pidgin=yes
   PIDGIN_DATADIR="`$PKG_CONFIG --variable=datadir pidgin`"],
  [have_pidgin=no
   PIDGIN_DATADIR="$datadir"])

AC_SUBST([PIDGIN_DATADIR])

AC_ARG_WITH([pidgin],
  AC_HELP_STRING([--with-pidgin],[enable support for some pidgin dialogs]),
  [],
  [])

use_pidgin=no

AS_IF(
  [test "x$with_pidgin" = xno],
    [],
  [test "x$have_pidgin" != xno],
    [
      AC_DEFINE(SPIN_USE_PIDGIN,1,[wether pidgin spezific features should be used])
      use_pidgin=yes
    ],
  [test "x$with_pidgin" != x],
    [
      AC_MSG_ERROR([pidgin not found])
    ])

AM_CONDITIONAL(USE_PIDGIN,[test "x$use_pidgin" != xno])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile po/Makefile.in pixmaps/Makefile])

AC_OUTPUT