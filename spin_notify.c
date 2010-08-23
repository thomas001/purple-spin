/* Copyright 2009 Thomas Weidner */

/* This file is part of Purple-Spin. */

/* Purple-Spin is free software: you can redistribute it and/or modify */
/* it under the terms of the GNU General Public License as published by */
/* the Free Software Foundation, either version 3 of the License, or */
/* (at your option) any later version. */

/* Purple-Spin is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
/* GNU General Public License for more details. */

/* You should have received a copy of the GNU General Public License */
/* along with Purple-Spin.  If not, see <http://www.gnu.org/licenses/>. */

#include "spin_notify.h"

#if SPIN_USE_PIDGIN

#include "gtkblist.h"
#include "gtkutils.h"

static void spin_notify_vmessage(SpinData* spin,
				 const gchar* title,
				 const gchar* fmt,va_list args)
{
  g_return_if_fail(spin);
  g_return_if_fail(fmt);
  
  gchar* text = g_strdup_vprintf(fmt,args);
  GtkWidget* alert = pidgin_make_mini_dialog(spin->gc,GTK_STOCK_DIALOG_INFO,
					     title,text,NULL,
					     _("close"),NULL,
					     NULL);
  pidgin_blist_add_alert(alert);
  g_free(text);
}

#else

static void spin_notify_vmessage(SpinData* spin,
				 const gchar* title,
				 const gchar* fmt,va_list args)
{
  g_return_if_fail(spin);
  g_return_if_fail(fmt);
  
  gchar* text = g_strdup_vprintf(fmt,args);
  purple_notify_info(spin->gc,title,text,NULL);
  g_free(text);
}

#endif

static void spin_notify_message(SpinData* spin,const gchar* title,
				const gchar* fmt,...)
{
  va_list ap;
  va_start(ap,fmt);
  spin_notify_vmessage(spin,title,fmt,ap);
  va_end(ap);
}

void spin_notify_nick_changed(SpinData* spin,const gchar* old_nick,
			      const gchar* new_nick)
{
  spin_notify_message(spin,_("Nick changed!"),_("%s is now known as %s"),
		      old_nick,new_nick);
}


void spin_notify_nick_removed(SpinData* spin,const gchar* nick)
{
  spin_notify_message(spin,_("Friend removed!"),
		      _("%s is not any more on your friend list"),nick);
}

void spin_notify_game_invite(SpinData* spin,const gchar* nick,
			     const gchar* title,const gchar* t,
			     const gchar* id,const gchar* passwd)
{
  spin_notify_message(spin,_("Invitation!"),
		      _("%s has invited you to a game %s"),nick,title);
}

void spin_notify_guestbook_entry(SpinData* spin,gchar* who)
{
  spin_notify_message(spin,_("New guestbook entry!"),
		      _("%s has added a new entry to your guestbook"),who);
}

void spin_notify_gift(SpinData* spin,gchar* who)
{
  spin_notify_message(spin,_("New gift!"),_("%s has given you something"),
		      who);
}




