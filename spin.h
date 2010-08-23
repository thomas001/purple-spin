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

#ifndef PURPLE_SPIN_H_
#define PURPLE_SPIN_H_

#ifdef HAVE_CONFIG_H_
# include "config.h"
#endif

#include <glib.h>
#define GETTEXT_PACKAGE "purple-spin"
#include <glib/gi18n-lib.h>

#include "account.h"
#include "circbuffer.h"

typedef enum
  {
    SPIN_STATE_GOT_WEB_LOGIN = (1<<0),
    SPIN_STATE_GOT_CHAT_LOGIN = (1<<1),
    SPIN_STATE_GOT_INITIAL_FRIEND_LIST = (1<<2),
    SPIN_STATE_GOT_INITIAL_MAIL_LIST = (1<<3),
    SPIN_STATE_GOT_INITIAL_PREFS = (1<<4),
    SPIN_STATE_GOT_INITIAL_BLOCK_LIST = (1<<5), // NOT used in called code
    SPIN_STATE_ALL_CONNECTION_STATES = ((1<<5)-1)
  } SpinConnectionState;

struct _SpinData
{
  PurpleConnection* gc;
  gint fd;

  GString* inbuf;
  PurpleCircBuffer* outbuf;

  gchar* session;
  guint write_handle,read_handle;
  guint login_timeout_handle, ping_timeout_handle;
  SpinConnectionState state;
  PurpleRoomlist* roomlist;

  gchar* username;
  gchar* normalized_username;
  GRegex* nick_regex;

  GHashTable* pending_joins;
  GHashTable* updated_status_list;
};
typedef struct _SpinData SpinData;

void spin_set_status(PurpleAccount* account,PurpleStatus* status);
void spin_write_command(SpinData* spin,gchar cmd,...) G_GNUC_NULL_TERMINATED;
void spin_start_read(SpinData* spin);
gchar* spin_encode_user(const gchar* user);

gchar* spin_convert_in_text(const gchar* text);
gchar* spin_convert_out_text(const gchar* text);

gchar* spin_session_url(SpinData* spin,const gchar* targetfmt,...);
gchar* spin_url(SpinData* spin,const gchar* fmt,...);

const gchar* spin_format_out_msg(const gchar* msg,const gchar** ty,
				 gboolean* free);

#endif
