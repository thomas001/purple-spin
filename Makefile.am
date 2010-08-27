plugindir = @PURPLE_PLUGINDIR@
plugin_LTLIBRARIES = libspin.la

libspin_la_SOURCES = spin.c spin_actions.c spin_chat.c spin_friends.c spin_login.c spin_mail.c spin_notify.c spin_parse.c spin_userinfo.c spin_web.c spin_prefs.c spin_cmds.c spin_privacy.c
noinst_HEADERS  = spin.h spin_actions.h spin_chat.h spin_friends.h spin_login.h spin_mail.h spin_notify.h spin_parse.h spin_userinfo.h spin_web.h spin_prefs.h spin_cmds.h spin_privacy.h

libspin_la_CFLAGS = @CFLAGS@ @PURPLE_CFLAGS@ @GLIB_CFLAGS@ @JSON_GLIB_CFLAGS@
libspin_la_CPPFLAGS = @XML_CPPFLAGS@ -DLOCALEDIR=\"$(localedir)\"

libspin_la_LDFLAGS = -module -avoid-version -shared @LDFLAGS@ @PURPLE_LIBS@ @GLIB_LIBS@ @JSON_GLIB_LIBS@ @XML_LIBS@ @LIBINTL@

SUBDIRS = po
ACLOCAL_AMFLAGS = -I m4

#EXTRA_DIST = config.rpath m4/ChangeLog 

if USE_PIDGIN
libspin_la_CFLAGS += @PIDGIN_CFLAGS@
libspin_la_LDFLAGS += @PIDGIN_LIBS@
SUBDIRS += pixmaps
endif

