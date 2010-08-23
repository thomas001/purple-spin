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

#define PURPLE_PLUGINS

#include "accountopt.h"
#include "blist.h"
#include "plugin.h"
#include "util.h"
#include "version.h"
#include "network.h"

#ifdef _WIN32
#include "win32dep.h" /* for LOCALEDIR */
#endif

#include "accountopt.h"
#include "blist.h"
#include "conversation.h"
#include "dnsquery.h"
#include "debug.h"
#include "notify.h"
#include "privacy.h"
#include "prpl.h"
#include "plugin.h"
#include "util.h"
#include "version.h"
#include "network.h"
#include "xmlnode.h"

#include "dnssrv.h"
#include "ntlm.h"

#include "spin.h"
#include "spin_parse.h"
#include "spin_chat.h"
#include "spin_login.h"
#include "spin_actions.h"
#include "spin_userinfo.h"
#include "spin_cmds.h"
/* #include "spin_privacy.h" */

#include <unistd.h>
#include <errno.h>
#include <string.h>
#ifdef WIN32
#  include <winsock2.h>
#else
#  include <sys/socket.h>
#endif

static const char* spin_list_icon(PurpleAccount* a G_GNUC_UNUSED,
				  PurpleBuddy* b G_GNUC_UNUSED)
{
  return "spin";
}

static GList* spin_status_types(PurpleAccount* a G_GNUC_UNUSED)
{
  PurpleStatusType* type;
  GList* types = NULL;

  type = purple_status_type_new_full(PURPLE_STATUS_AVAILABLE,NULL,NULL,
				     TRUE,TRUE,FALSE);
  types = g_list_append(types,type);

  type = purple_status_type_new_full(PURPLE_STATUS_OFFLINE,NULL,NULL,
				     TRUE,TRUE,FALSE);
  types = g_list_append(types,type);

  type = purple_status_type_new_with_attrs
    (PURPLE_STATUS_AWAY,NULL,NULL,TRUE,TRUE,FALSE,
     "message","MESSAGE",purple_value_new(PURPLE_TYPE_STRING),
     NULL);
  types = g_list_append(types,type);

  return types;
}

gchar* spin_encode_user(const gchar* user)
{
  g_return_val_if_fail(user,NULL);

  gsize bytes_in,bytes_out,len = strlen(user);
  GError* error = NULL;
  gchar* out = g_convert(user,len,"ISO-8859-15","UTF-8",&bytes_in,&bytes_out,
			 &error);
  if(error || bytes_in != len)
    {
      g_error_free(error);
      if(out)
	g_free(out);
      return NULL;
    }
  return out;
}

gchar* spin_convert_in_text(const gchar* text)
{
  g_return_val_if_fail(text,NULL);
  /* gchar* escaped; */

  if(!g_utf8_validate(text,-1,NULL))
    {
      gsize bytes_in,bytes_out,len = strlen(text);
      GError* error = NULL;
      gchar* out = g_convert(text,len,"UTF-8","ISO-8859-15",&bytes_in,
			     &bytes_out,&error);
      if(error || bytes_in != len)
	{
	  g_error_free(error);
	  if(out)
	    g_free(out);
	  return NULL;
	}
      /* escaped = g_markup_escape_text(out,-1); */
      /* g_free(out); */
      return out;
    }
  else
    return g_strdup(text);
  /*   escaped = g_markup_escape_text(text,-1); */
  
  /* return escaped; */
}

gchar* spin_convert_out_text(const gchar* text)
{
  gchar* no_html = purple_markup_strip_html(text);
  gchar* no_entities =  purple_unescape_html(no_html);
  g_free(no_html);
  return no_entities;
}

gchar* spin_session_url(SpinData* spin,const gchar* targetfmt,...)
{
  g_return_val_if_fail(targetfmt,NULL);
  g_return_val_if_fail(spin,NULL);
  g_return_val_if_fail(spin->session,NULL);

  va_list ap;
  va_start(ap,targetfmt);
  gchar* target = g_strdup_vprintf(targetfmt,ap);
  va_end(ap);
  gchar* escaped = g_uri_escape_string(target,NULL,FALSE);
  gchar* uri = g_strdup_printf("http://www.spin.de/login/setsession?"
			       "session=%s&target=%s",spin->session,escaped);
  g_free(escaped);
  g_free(target);
  return uri;
}

gchar* spin_url(SpinData* spin,const gchar* fmt,...)
{
  g_return_val_if_fail(spin,NULL);
  g_return_val_if_fail(fmt,NULL);

  va_list ap;
  va_start(ap,fmt);
  gchar* loc = g_strdup_vprintf(fmt,ap);
  va_end(ap);
  gchar* uri = g_strdup_printf("http://www.spin.de%s%s",
			       loc[0] == '/' ? "" : "/",loc);
  g_free(loc);
  return uri;
}

static void spin_try_parse(SpinData* spin)
{
  gchar* i = spin->inbuf->str,*j=i;

  for(;i != spin->inbuf->str + spin->inbuf->len; ++i)
    {
      switch(*i)
	{
	case '\0':
	  *i = ' ';
	  break;
	case '\n':
	  *i = '\0';
	  spin_parse_line(spin,j);
	  j = i + 1;
	  break;
	}
    }

  g_string_erase(spin->inbuf, 0, j - spin->inbuf->str);
}

static gboolean check_socket_error(PurpleConnection* gc,ssize_t ret)
{
#ifdef WIN32
  int err = WSAGetLastError();
  if(ret < 0 && err == WSAEWOULDBLOCK)
    return FALSE;
  if(ret < 0)
    {
      gchar* buf;
      FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
		     NULL,
		     err,
		     0,
		     (LPSTR) &buf,
		     0,NULL);
      purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				     buf);
      LocalFree(buf);
      return TRUE;
    }
  return FALSE;
#else
  if(ret < 0 && errno == EAGAIN)
    return FALSE;
  else if(ret <= 0)
    {
      purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				     g_strerror(errno));
      return TRUE;
    }
  else
    return FALSE;
#endif
}

static void write_cb(gpointer data,gint fd,
		     PurpleInputCondition cond G_GNUC_UNUSED)
{
  PurpleConnection* gc = (PurpleConnection*) data;
  SpinData* spin = (SpinData*) gc->proto_data;

  if(!spin)
    return;

  guint to_write = purple_circ_buffer_get_max_read(spin->outbuf);

  if(to_write == 0)
    {
      purple_input_remove(spin->write_handle);
      spin->write_handle = 0;
      return;
    }

  // ssize_t written = write(fd,spin->outbuf->outptr,to_write);
  ssize_t written = send(fd,spin->outbuf->outptr,to_write,0);
  if(check_socket_error(gc,written))
     return;

  purple_circ_buffer_mark_read(spin->outbuf,written);
}

void spin_write_command(SpinData* spin,gchar cmd,...)
{
  GString* out = g_string_new("");
  const gchar* arg;
  g_string_append_c(out,cmd);

  va_list ap;
  va_start(ap,cmd);
  int first = 1;
  while((arg = va_arg(ap,const gchar*)))
    {
      if(!first)
	g_string_append_c(out,'#');
      else
	first = 0;
      g_string_append(out,arg);
    }
  va_end(ap);

  gsize i;
  for(i = 0; i < out->len; ++i)
    if(out->str[i] == '\n')
      out->str[i] = ' ';

  g_string_append_c(out,'\n');

  purple_circ_buffer_append(spin->outbuf,out->str,out->len);

  if(!spin->write_handle)
    spin->write_handle = purple_input_add(spin->fd,PURPLE_INPUT_WRITE,write_cb,spin->gc);
}

static void read_cb(gpointer data,gint fd,
		    PurpleInputCondition cond G_GNUC_UNUSED)
{
  PurpleConnection* gc = (PurpleConnection*) data;
  SpinData* spin = (SpinData*) gc->proto_data;

  gchar buf[1024];
  //ssize_t nread = read(fd, buf, 1024);
  ssize_t nread = recv(fd, buf, 1024, 0);
  if(check_socket_error(gc,nread))
    return;

  if(spin)
    {
      g_string_append_len(spin->inbuf,buf,nread);
      spin_try_parse(spin);
    }
}

void spin_start_read(SpinData* spin)
{
  if(spin->read_handle)
    return;
  spin->read_handle = purple_input_add(spin->fd, PURPLE_INPUT_READ, read_cb,spin->gc);
}

static int spin_send_im(PurpleConnection* gc,const char* who,
			const char* msg,
			PurpleMessageFlags flags G_GNUC_UNUSED)
{
  SpinData* spin = (SpinData*) gc->proto_data;
  if(!spin->fd)
    return /*-ENOTCONN;*/ -1;

  gchar* who_2;
  if(!(who_2 = spin_encode_user(who)))
    return -1;
  
  gchar* msg_2 = spin_convert_out_text(msg);
  
  const gchar* ty = "a";
  if(purple_message_meify(msg_2,-1))
    ty = "c";
  
  spin_write_command(spin,'h',who_2,"0",ty,msg_2,NULL);
  
  g_free(msg_2);
  g_free(who_2);
  return 1;
}

void spin_set_status(PurpleAccount* account,PurpleStatus* status)
{
  PurpleConnection* gc = purple_account_get_connection(account);
  if(!gc || !PURPLE_CONNECTION_IS_VALID(gc) || !purple_status_is_active(status))
    return;
  SpinData* spin = (SpinData*) gc->proto_data;
  PurpleStatusType* type = purple_status_get_type(status);
  PurpleStatusPrimitive prim = purple_status_type_get_primitive(type);
  purple_debug_info("spin","SET STATUS: %i\n",prim);
  const gchar* msg,*room_away_state = NULL;
  switch(prim)
    {
    case PURPLE_STATUS_AVAILABLE:
      spin_write_command(spin,'W',"2","a","",NULL);
      room_away_state = "0#away#0";
      break;
    case PURPLE_STATUS_AWAY:
      msg = purple_status_get_attr_string(status,"message");
      if(!msg)
	msg = _("user is away");
      gchar* msg_2 = spin_convert_out_text(msg);
      spin_write_command(spin,'W',"1","a",msg_2,NULL);
      g_free(msg_2);
      room_away_state = "0#away#1";
      break;
    default:
      ;
    }

  GList* chat;
  for(chat = purple_get_chats();chat;chat = g_list_next(chat))
    {
      PurpleConversation* conv = (PurpleConversation*) chat->data;
      if(purple_conversation_get_account(conv) != account)
	continue;
      const gchar* name = purple_conversation_get_name(conv);
      spin_chat_set_room_status(spin,name,status);
    }
  
}

static gboolean spin_ping_timeout(gpointer data)
{
  PurpleConnection* gc = (PurpleConnection*) data;
  purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				 _("ping timeout"));
  return FALSE;
}

static void spin_keepalive(PurpleConnection* gc)
{
  SpinData* spin = (SpinData*) gc->proto_data;
  spin_write_command(spin,'J',"p",NULL);
  if(!spin->ping_timeout_handle)
    spin->ping_timeout_handle = purple_timeout_add_seconds(60,spin_ping_timeout,gc);
}

static void spin_tooltip_text(PurpleBuddy *buddy,
			      PurpleNotifyUserInfo *user_info,
			      gboolean full G_GNUC_UNUSED)
{
  PurplePresence* presence = purple_buddy_get_presence(buddy);
  PurpleStatus* status = purple_presence_get_active_status(presence);
  const gchar* msg = purple_status_get_attr_string(status,"message");

  if(msg)
    purple_notify_user_info_add_pair(user_info,"Away",msg);
}

static gchar* spin_status_text(PurpleBuddy* buddy)
{
  PurplePresence* presence = purple_buddy_get_presence(buddy);
  PurpleStatus* status = purple_presence_get_active_status(presence);
  const gchar* msg = purple_status_get_attr_string(status,"message");
  return g_strdup(msg);
}

static const gchar* spin_normalize(const PurpleAccount* account G_GNUC_UNUSED,
				   const gchar* name)
{
  static gchar buf[2048];
  gchar* down = g_ascii_strdown(name,-1);
  gchar* normalized = g_utf8_normalize(down,-1,G_NORMALIZE_DEFAULT_COMPOSE);
  g_strlcpy(buf,normalized,2048);
  g_free(normalized);
  g_free(down);
  return buf;
}


static PurplePluginProtocolInfo prpl_info =
{
	OPT_PROTO_CHAT_TOPIC,
	NULL,					/* user_splits */
	NULL,					/* protocol_options */
	NO_BUDDY_ICONS,			/* icon_spec */
	spin_list_icon,		                /* list_icon */
	NULL,					/* list_emblems */
	spin_status_text,			/* status_text */
	spin_tooltip_text,			/* tooltip_text */
	spin_status_types,	                /* away_states */
	NULL,					/* blist_node_menu */
	spin_chat_info,				/* chat_info */
	spin_chat_info_defaults,		/* chat_info_defaults */
	spin_login,			/* login */
	spin_close,			/* close */
	spin_send_im,			/* send_im */
	NULL,					/* set_info */
	NULL,			/* send_typing */
	spin_get_info,					/* get_info */
	spin_set_status,		/* set_status */
	NULL,					/* set_idle */
	NULL,					/* change_passwd */
	NULL,		/* add_buddy */
	NULL,					/* add_buddies */
	NULL,	/* remove_buddy */
	NULL,					/* remove_buddies */
	NULL /*spin_add_permit*/,		/* add_permit */
	NULL /*spin_add_deny*/,			/* add_deny */
	NULL /*spin_rem_permit*/,		/* rem_permit */
	NULL /*spin_rem_deny*/,			/* rem_deny */
	NULL /*spin_set_permit_deny*/, 		/* set_permit_deny */
	spin_chat_join,				/* join_chat */
	NULL,					/* reject_chat */
	spin_get_chat_name,			/* get_chat_name */
	NULL,					/* chat_invite */
	spin_chat_leave,			/* chat_leave */
	NULL,					/* chat_whisper */
	spin_chat_send,				/* chat_send */
	spin_keepalive,	                	/* keepalive */
	NULL,					/* register_user */
	NULL,					/* get_cb_info */
	NULL,					/* get_cb_away */
	NULL,					/* alias_buddy */
	NULL,					/* group_buddy */
	NULL,					/* rename_group */
	NULL,					/* buddy_free */
	NULL,					/* convo_closed */
	spin_normalize,		/* normalize */
	NULL,					/* set_buddy_icon */
	NULL,					/* remove_group */
	NULL,					/* get_cb_real_name */
	NULL,					/* set_chat_topic */
	NULL,					/* find_blist_chat */
	spin_roomlist_get_list,			/* roomlist_get_list */
	spin_roomlist_cancel,			/* roomlist_cancel */
	NULL,					/* roomlist_expand_category */
	NULL,					/* can_receive_file */
	NULL,					/* send_file */
	NULL,					/* new_xfer */
	NULL,					/* offline_message */
	NULL,					/* whiteboard_prpl_ops */
	NULL,		/* send_raw */
	NULL,					/* roomlist_room_serialize */

	/* padding */
	NULL,
	NULL,
	NULL,
	sizeof(PurplePluginProtocolInfo),       /* struct_size */
	NULL
};

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_PROTOCOL,                           /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,                          /**< priority       */

	"prpl-spin",                                      /**< id             */
	"SPIN",                                         /**< name           */
	"0.1",                                            /**< version        */
	"Spinchat Protocol Plugin",                   /**  summary        */
	"The Spinchat Protocol Plugin",               /**  description    */
	"Thomas Weidner <thomas_weidner@gmx.de>",         /**< author         */
	"http://www.spin.de",                             /**< homepage       */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	NULL,
	spin_actions,

	/* padding */
	NULL,
	NULL,
	NULL,
	NULL
};

static void init_plugin(PurplePlugin* plugin G_GNUC_UNUSED)
{
  PurpleAccountOption* option;
  GList* ol = NULL;

#ifdef ENABLE_NLS
  bindtextdomain(GETTEXT_PACKAGE,LOCALEDIR);
  bind_textdomain_codeset(GETTEXT_PACKAGE,"UTF-8");
#endif

  option = purple_account_option_string_new(_("Server"),"server","www.spin.de");
  ol = g_list_append(ol,option);
  option = purple_account_option_int_new(_("Port"),"port",3003);
  ol = g_list_append(ol,option);

  PurpleKeyValuePair* pair;
  GList* vals = NULL;
  pair = g_new(PurpleKeyValuePair,1);
  pair->key = g_strdup(_("Always"));
  pair->value = g_strdup("always");
  vals = g_list_append(vals,pair);

  pair = g_new(PurpleKeyValuePair,1);
  pair->key = g_strdup(_("Never"));
  pair->value = g_strdup("never");
  vals = g_list_append(vals,pair);

  pair = g_new(PurpleKeyValuePair,1);
  pair->key = g_strdup(_("Only for non-buddys"));
  pair->value = g_strdup("non-buddys");
  vals = g_list_append(vals,pair);

  option = purple_account_option_list_new(_("Show away auto replies"),
					  "show-away",vals);
  ol = g_list_append(ol,option);

  option = purple_account_option_string_new(_("Nickname regular expression"),
					    "nick-regex","");
  ol = g_list_append(ol, option);

  option = purple_account_option_bool_new(_("Use secure login"),
					  "secure-login",TRUE);
  ol = g_list_append(ol, option);

  prpl_info.protocol_options = ol;

  /* GList* splits = NULL; */
  /* PurpleAccountUserSplit*split; */
  /* split = purple_account_user_split_new("Server","www.spin.de",'#'); */
  /* splits = g_list_append(splits,split); */
  /* split = purple_account_user_split_new("Port","3003",'#'); */
  /* splits = g_list_append(splits,split); */
  /* prpl_info.user_splits = splits; */

  spin_register_commands();

}

PURPLE_INIT_PLUGIN(spin,init_plugin,info)
