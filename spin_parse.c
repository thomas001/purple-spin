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

#include "spin_parse.h"
#include "spin_friends.h"

#include "debug.h"
#include "connection.h"
#include "server.h"
#include "spin_notify.h"
#include "spin_mail.h"
#include "spin_chat.h"
/* #include "spin_privacy.h" */
#include "spin_prefs.h"
#include "spin_login.h"

#include <string.h>

static const gchar* leave_reasons[] =
  {
    N_("left the room"),
    N_("kicked by %3$s%1$.0s%2$.0s"),
    N_("kicked by the server"),
    N_("you are banned"),
    N_("room %2$s is currently full%1$.0s"),
    N_("room %2$s is closed%1$.0s"),
    N_("creation of new rooms is not allowed"),
    N_("room is for registered users only"),
    N_("room is for VIP users only"),
    N_("room could not be created: too many rooms"),
    N_("room could not be created: room name is illegal"),
    N_("too long inactive"),
    N_("kicked of the server")
  };

static gchar* simple_strsep(gchar** in,gchar c)
{
  if(!in || !*in)
    return NULL;

  gchar* p = strchr(*in,c),*q=*in;
  if(p)
    {
      *p = '\0';
      *in = p+1;
      return q;
    }
  else
    {
      *in = NULL;
      return q;
    }
}

static G_GNUC_NULL_TERMINATED void spin_split_line(gchar* line,...) 
{
  va_list ap;
  gchar **p,**q = NULL;
  va_start(ap,line);
  while((p = va_arg(ap,gchar**)))
    {
      if(q)
	*q = simple_strsep(&line,'#');
      q = p;
      *p = line;
    }
  va_end(ap);
}

static gchar* spin_convert_user(SpinData* spin,gchar* user)
{
  g_return_val_if_fail(user,NULL);
  
  gsize bytes_in,bytes_out,len = strlen(user);
  GError* error = NULL;
  gchar* out = g_convert(user,len,"UTF-8","ISO-8859-15",&bytes_in,&bytes_out,
			 &error);
  if(error || bytes_in != len)
    {
      if(out)
	g_free(out);
      purple_connection_error_reason
	(spin->gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
	 _("invalid coded room name received"));
      
      return NULL;
    }
  return out;
}

gchar* spin_write_chat(gchar ty,const gchar* user,const gchar* t)
{
  static GRegex* me_re;
  if(!me_re) /* RACE CONDITION */
    me_re = g_regex_new("[/.]me",G_REGEX_OPTIMIZE,0,NULL);

  gchar *text,*text2;
  switch(ty)
    {
    case 'c':
    case 'd':
      /* text = g_strconcat("/me ",t,NULL); */
      return g_markup_printf_escaped("/me %s",t);
    case 'e':
    case 'f':
      text =  g_regex_replace_literal(me_re,t,-1,0,user,0,NULL);
      text2 = g_markup_printf_escaped("/me %s <i>[echo]</i>",text);
      g_free(text);
      return text2;
    default:
      return g_markup_escape_text(t,-1);
    }
}

PurpleConvChatBuddyFlags spin_get_flags(const gchar* r)
{
  g_return_val_if_fail(r,PURPLE_CBFLAGS_NONE);
  
  gchar* endp;
  gint64 v = g_ascii_strtoll(r,&endp,10);
  if(*endp) /* invalid number */
    return PURPLE_CBFLAGS_NONE;
  guint flags = PURPLE_CBFLAGS_NONE;
  if(v & 0x10)
    /* return PURPLE_CBFLAGS_OP;*/
    flags |= PURPLE_CBFLAGS_OP;
  /* 0x08 ????*/
  if(v & 0x4)
    /* return PURPLE_CBFLAGS_HALFOP;*/
    flags |= PURPLE_CBFLAGS_HALFOP;
  /* v & 0x2 is registered user*/
  if(v & 0x1)
    /* return PURPLE_CBFLAGS_VOICE;*/
    flags |= PURPLE_CBFLAGS_VOICE;
  return flags;
}

static void spin_chat_notfound(SpinData* spin,const gchar* room,const gchar* raw_room)
{
  purple_debug_info("spin","room not found: %s\n",room);
  /* try to leave the room */
  spin_write_command(spin,'d',raw_room,NULL);
}

static void spin_handle_connected(SpinData* spin,gchar* rest)
{
  PurpleAccount* account = purple_connection_get_account(spin->gc);
  purple_debug_info("spin","connected\n");
  spin_connect_add_state(spin,SPIN_STATE_GOT_CHAT_LOGIN);

  spin_set_status(account,purple_account_get_active_status(account));

  purple_connection_update_progress(spin->gc,Q_("Progress|Receiving Prefs"),
				    1,4);
  spin_receive_friends(spin);
  spin_check_mail(spin);
  /*spin_sync_privacy_lists(spin);*/
  spin_load_prefs(spin);
}

static void spin_handle_disconnected(SpinData* spin,gchar* rest)
{
  purple_debug_info("spin","disconnected\n");
  if(purple_connection_get_state(spin->gc) == PURPLE_CONNECTING)
    purple_connection_error_reason
      (spin->gc,PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
       _("chat server denied login"));
  else
    ; //purple_connection_set_state(spin->gc,PURPLE_DISCONNECTED);
}

static void spin_handle_null_msg(SpinData* spin,gchar* user,gchar* r,gchar* t)
{
  PurpleAccount* account = purple_connection_get_account(spin->gc);
  gchar *ty,*args,*escaped_args = NULL;
  spin_split_line(t,&ty,&args,NULL); /* args already utf8,ensured by caller */
  
  if(g_ascii_strcasecmp(ty,"away") == 0)
    {
      PurpleConversation* conv =
	purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,user,account);
      if(!conv)
	goto exit;
      gboolean show = TRUE;
      const gchar* setting = purple_account_get_string(account,"show-away",
						       "always");
      if(g_strcmp0(setting,"never") == 0)
	show = FALSE;
      else if(g_strcmp0(setting,"non-buddys") == 0)
	{
	  if(purple_find_buddy(account,user))
	    show = FALSE;
	}

      if(show)
	{
	  escaped_args = g_markup_escape_text(args,-1);
	  purple_conv_im_write(PURPLE_CONV_IM(conv),user,escaped_args,
			       PURPLE_MESSAGE_AUTO_RESP,time(NULL));
	}
    }
  else if(g_ascii_strcasecmp(ty,"ping") == 0)
    {
      gchar* encoded_user = spin_encode_user(user);
      if(!encoded_user)
	goto exit;
      spin_write_command(spin,'h',encoded_user,"2#0#pong",NULL);
      g_free(encoded_user);
    }
  else if(g_ascii_strcasecmp(ty,"nospam") == 0)
    {
      purple_notify_warning(spin->gc,"spam warning",args ? args : "no spam!",
			    user);
    }
  else if(g_ascii_strcasecmp(ty,"invite") == 0
	  && g_str_has_prefix(args,"game#"))
    {
      gchar *what,*name,*id,*t,*passwd;
      spin_split_line(args,&what,&name,&id,&t,&passwd,NULL);
      spin_notify_game_invite(spin,user,name,t,id,passwd);
    }

 exit:;
}

static void spin_handle_private_msg(SpinData* spin,gchar* rest)
{
  gchar *raw_user,*echo,*r,*ty,*raw_t,*t = NULL,*user=NULL,*written_t = NULL;
  spin_split_line(rest,&raw_user,&echo,&r,&ty,&raw_t,NULL);
  if(*echo != '0')
    return;
  if(!(user = spin_convert_user(spin,raw_user))
     || !(t = spin_convert_in_text(raw_t)))
    goto exit;

  if(*ty == '0')
    {
      spin_handle_null_msg(spin,user,r,t);
      goto exit;
    }

  written_t = spin_write_chat(*ty,user,t);

  serv_got_im(spin->gc,user,written_t,0,time(NULL));

 exit:
  g_free(written_t);
  g_free(t);
  g_free(user);
}

static void spin_handle_ping(SpinData* spin,gchar* rest)
{
  if(g_strcmp0(rest,"p") != 0)
    return;
  if(spin->ping_timeout_handle)
    {
      purple_timeout_remove(spin->ping_timeout_handle);
      spin->ping_timeout_handle = 0;
    }
}

static void spin_handle_single_status(SpinData* spin,gchar ty,gchar* rest)
{
  gchar *raw_user,*raw_reason,*user=NULL,*reason=NULL;
  PurpleAccount* account = purple_connection_get_account(spin->gc);

  spin_split_line(rest,&raw_user,&raw_reason,NULL);
  if(!(user = spin_convert_user(spin,raw_user))
     || (raw_reason && !(reason = spin_convert_in_text(raw_reason))))
    goto exit;

  PurpleBuddy *buddy = purple_find_buddy(account,user);
  if(!buddy)
    goto exit;

  if(ty == 'i')
    {
      purple_prpl_got_user_status(account,user,"away",
				  "message",reason ? reason : "",NULL);
    }
  /* the javascript code treats away and online seperate, so receiving an
     online message does not reset away state */
  else if(ty == 'j'
	  || (ty == 'g'
	      && !purple_presence_is_online(purple_buddy_get_presence(buddy))))
    {
      purple_prpl_got_user_status(account,user,"available",NULL);
    }
  else if(ty == 'h')
    {
      purple_prpl_got_user_status(account,user,"offline",NULL);
    }

  g_hash_table_insert(spin->updated_status_list,
		      g_strdup(purple_normalize(account,user)),
		      GINT_TO_POINTER(1));

 exit:
  g_free(user);
  g_free(reason);
}

static void spin_handle_status_list(SpinData* spin,gchar* rest)
{
  /* ignore this as we query over http *after* login */
}

static void spin_handle_status(SpinData* spin,gchar* rest)
{
  gchar *ty,*raw_args;
  spin_split_line(rest,&ty,&raw_args,NULL);

  if(!ty)
    return;

  if(*ty == 'g' || *ty == 'h' || *ty == 'j' || *ty == 'i')
    spin_handle_single_status(spin,*ty,raw_args);
  else if(*ty == 'e')
    spin_handle_status_list(spin,raw_args);
}

static void spin_handle_notify(SpinData* spin,gchar* rest)
{
  gchar *ty,*args,*user,*user_2=NULL;
  spin_split_line(rest,&ty,&user,&args,NULL);

  if(!ty)
    return;

  switch(*ty)
    {
    case 'l': /* reload friends */
      spin_receive_friends(spin);
      break;
    case 'i': /* room invited */
      break;
    case 'j': /* chat with user invited */
      break;
    case 'a': /* new mail */
      spin_check_mail(spin);
      break;
    case 'b': /* new guestbook */
    case 'c': /* new gift */
      if((user_2 = spin_convert_user(spin,user)))
	{
	  if(*ty == 'b')
	    spin_notify_guestbook_entry(spin,user);
	  else
	    spin_notify_gift(spin,user);
	  g_free(user_2);
	}
      break;
    }
}

static void spin_handle_list(SpinData* spin,gchar* rest)
{
  if(!spin->roomlist)
    return;
  gchar* room_name;

  while((room_name = simple_strsep(&rest,'#')))
    {
      if(!(room_name = spin_convert_user(spin,room_name)))
	continue;
      PurpleRoomlistRoom* room =
	purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM,room_name,NULL);
      purple_roomlist_room_add_field(spin->roomlist,room,room_name);
      purple_roomlist_room_add(spin->roomlist,room);
      g_free(room_name);
    }

  purple_roomlist_set_in_progress(spin->roomlist,FALSE);
  purple_roomlist_unref(spin->roomlist);
  spin->roomlist = NULL;
}

static void spin_handle_joinleave(SpinData* spin,gchar* rest)
{
  PurpleAccount* account = purple_connection_get_account(spin->gc);
  gchar *raw_room,*st,*raw_u1,*raw_u2,*r,*other,*ip,*m,*u1=NULL,*u2=NULL,*room=NULL;
  static int id = 1;
  spin_split_line(rest,&raw_room,&st,&raw_u1,&raw_u2,&r,&other,&ip,&m,NULL);
  gchar *reason = NULL, *normalized_room = NULL;

  if(!(room = spin_convert_user(spin,raw_room))
       || !(u1 = spin_convert_user(spin,raw_u1))
       || !(u2 = spin_convert_user(spin,raw_u2)))
    goto exit;

  normalized_room = g_strdup(purple_normalize(account,room));

  if(g_ascii_islower(*st))
    { /* join */
      if(g_strcmp0(purple_normalize(account,u1),spin->normalized_username) == 0)
	{
	  g_hash_table_remove(spin->pending_joins,normalized_room);
	  serv_got_joined_chat(spin->gc,id++,normalized_room);
	  spin_write_command(spin,'j',raw_room,NULL);
	  spin_write_command(spin,'o',raw_room,NULL);
	  spin_chat_set_room_status(spin,room,purple_account_get_active_status(account));
	}
      else
	{
	  PurpleConversation* conv =
	    purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,room,
						  account);
	  if(!conv)
	    {
	      spin_chat_notfound(spin,room,raw_room);
	      goto exit;
	    }
	  purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv),u1,NULL,
				    spin_get_flags(r),TRUE);
	}
    }
  else
    { /* leave */
      if('B' <= *st && *st <= 'M')
	reason =
	  g_strdup_printf(g_dgettext(GETTEXT_PACKAGE,leave_reasons[*st-'A']),
			  u1,room,u2,ip);

      if(g_hash_table_lookup(spin->pending_joins,normalized_room))
	{
	  GHashTable* table = g_hash_table_new(g_str_hash,g_str_equal);
	  g_hash_table_insert(table,"room",normalized_room);
	  purple_serv_got_join_chat_failed(spin->gc,table);
	  g_hash_table_unref(table);

	  gchar* head = g_strdup_printf(_("cannot join room %s"),room);
	  purple_notify_error(spin->gc,head,
			      reason ? reason : _("no known reason"),NULL);
	  g_free(head);
	  g_hash_table_remove(spin->pending_joins,normalized_room);

	  goto exit;
	}
      
      PurpleConversation* conv =
	purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,room,
					      account);
      if(g_strcmp0(purple_normalize(account,u1),spin->normalized_username) == 0)
	{
	  if(conv)
	    {
	      gchar* msg = g_strdup_printf(_("You have left the room%s%s%s"),
					   reason ? " (" : "",
					   reason ? reason : "",
					   reason ? ")" : "");
	      purple_conv_chat_write(PURPLE_CONV_CHAT(conv),"",msg,
				     PURPLE_MESSAGE_SYSTEM|PURPLE_MESSAGE_NICK,
				     time(NULL));
	      serv_got_chat_left
		(spin->gc,purple_conv_chat_get_id(PURPLE_CONV_CHAT(conv)));
	      g_free(msg);
	    }
	}
      else
	{
	  if(conv)
	    purple_conv_chat_remove_user(PURPLE_CONV_CHAT(conv),u1,reason);
	  else
	    spin_chat_notfound(spin,room,raw_room);
	}
    }

 exit:
  g_free(reason);
  g_free(normalized_room);
  g_free(room);
  g_free(u1);
  g_free(u2);
}

static void spin_handle_chat_null_msg(SpinData* spin,gchar* room,gchar* u,
				      gchar* r,gchar* m)
{
  gchar *ty,*args,*who=NULL,*msg=NULL,*text=NULL,*escaped_text=NULL;
  spin_split_line(m,&ty,&args,NULL);

  PurpleAccount* account = purple_connection_get_account(spin->gc);

  PurpleConversation* conv =
    purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,
					  room,account);
  if(!conv)
    {
      gchar* raw_room = spin_encode_room(room);
      if(raw_room)
	spin_chat_notfound(spin,room,raw_room);
      g_free(raw_room);
      goto exit;
    }
  
  if(g_ascii_strcasecmp(ty,"warn") == 0)
    {
      if(!(spin_get_flags(r) & (PURPLE_CBFLAGS_OP | PURPLE_CBFLAGS_HALFOP)))
	goto exit;
      gchar *raw_who,*raw_msg;
      spin_split_line(args,&raw_who,&raw_msg,NULL);
      if(!(who = spin_convert_user(spin,raw_who))
	 || (raw_msg && !(msg = spin_convert_in_text(raw_msg))))
	goto exit;
      if(g_strcmp0(purple_normalize(account,who),spin->normalized_username)== 0)
	{
	  if(msg && *msg)
	    text = g_strdup_printf(_("You have been warned by %s: %s"),
				   u,msg);
	  else
	    text = g_strdup_printf(_("You have been warned by %s"),u);
	  escaped_text = g_markup_escape_text(text,-1);
	  g_free(text);
	  /* text is filled in the next line so the last free is ok */
	  text = g_strdup_printf("<span style=\"color:red\">%s</span>",
				 escaped_text);
	  purple_conv_chat_write(PURPLE_CONV_CHAT(conv),u,text,
				 PURPLE_MESSAGE_NICK|PURPLE_MESSAGE_SYSTEM,
				 time(NULL));
	}
      else
	{
	  if(msg && *msg)
	    text = g_strdup_printf(_("%s has been warned by %s: %s"),
				   who,u,msg);
	  else
	    text = g_strdup_printf(_("%s has been warned by %s"),who,u);
	  escaped_text = g_markup_escape_text(text,-1);
	  purple_conv_chat_write(PURPLE_CONV_CHAT(conv),u,escaped_text,
				 PURPLE_MESSAGE_SYSTEM,
				 time(NULL));
	}
    }

#if SPIN_USE_CBFLAGS_AWAY
  if(g_ascii_strcasecmp(ty,"away") == 0)
    {
      gchar *status,*dummy;
      spin_split_line(args,&status,&dummy,NULL);
      if(!status)
	goto exit;
      gint status_int = g_ascii_strtoll(status,NULL,10);
      PurpleConvChatBuddyFlags flags =
	purple_conv_chat_user_get_flags(PURPLE_CONV_CHAT(conv),u);
      if(status_int)
	flags |= PURPLE_CBFLAGS_AWAY;
      else
	flags &= ~PURPLE_CBFLAGS_AWAY;

      purple_conv_chat_user_set_flags(PURPLE_CONV_CHAT(conv),u,flags);
    }
#endif

 exit:
  g_free(escaped_text);
  g_free(text);
  g_free(who);
  g_free(msg);
}

static void spin_handle_chat_msg(SpinData* spin,gchar* rest)
{
  PurpleAccount* account = purple_connection_get_account(spin->gc);
  gchar *raw_room,*raw_u,*r,*ty,*raw_m,*room = NULL,*u = NULL,*m = NULL,
    *written_m = NULL;
  spin_split_line(rest,&raw_room,&raw_u,&r,&ty,&raw_m,NULL);
  if(!(room = spin_convert_user(spin,raw_room))
     || !(u = spin_convert_user(spin,raw_u))
     || !(m = spin_convert_in_text(raw_m)))
    goto exit;

  if(*ty == '0')
    {
      spin_handle_chat_null_msg(spin,room,u,r,raw_m);
      goto exit;
    }

  PurpleMessageFlags flags = 0;
  if(g_regex_match(spin->nick_regex,m,0,NULL))
    flags |= PURPLE_MESSAGE_NICK;

  written_m = spin_write_chat(*ty,u,m);

  PurpleConversation* conv =
    purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,room,
					  account);
  if(conv)
    serv_got_chat_in(spin->gc,purple_conv_chat_get_id(PURPLE_CONV_CHAT(conv)),
		     u,flags,written_m,time(NULL));
  else
    spin_chat_notfound(spin,room,raw_room);

 exit:
  g_free(written_m);
  g_free(m);
  g_free(u);
  g_free(room);
}

static void spin_handle_chatter_list(SpinData* spin,gchar* rest)
{
  PurpleAccount* account = purple_connection_get_account(spin->gc);
  char *raw_room,*entries,*room = NULL;
  GList *users=NULL,*flags=NULL;
  spin_split_line(rest,&raw_room,&entries,NULL);
  if(!(room = spin_convert_user(spin,raw_room)))
    goto exit;
  
  PurpleConversation* conv;
  conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,
					       room,account);
  if(!conv)
    {
      spin_chat_notfound(spin,room,raw_room);
      goto exit;
    }
  
  gchar* entry;
  while((entry = simple_strsep(&entries,'#')))
    {
      gchar* name = simple_strsep(&entry,':');
      gchar* mode = simple_strsep(&entry,':'); 
      gchar* state = simple_strsep(&entry,':');
      if(!name || !mode || !state)
	continue;
      if(!(name = spin_convert_user(spin,name)))
	continue;
      PurpleConvChatBuddyFlags user_flags = spin_get_flags(mode);
#if SPIN_USE_CBFLAGS_AWAY
      if(strchr(state,'a'))
	user_flags |= PURPLE_CBFLAGS_AWAY;
#endif
      users = g_list_append(users,name);
      flags = g_list_append(flags,GINT_TO_POINTER(user_flags));
    }
  purple_conv_chat_add_users(PURPLE_CONV_CHAT(conv),users,NULL,flags,FALSE);

 exit:;
  GList* i;
  for(i = users; i; i = i->next)
    g_free(i->data);

  g_list_free(users);
  g_list_free(flags);
  g_free(room);
}

static void spin_handle_msg_error(SpinData* spin,gchar* raw_user)
{
  PurpleAccount* account = purple_connection_get_account(spin->gc);
  gchar* user = NULL;
  if(!(user = spin_convert_user(spin,raw_user)))
    goto exit;

  purple_conv_present_error(user,account,_("error sending message"));

 exit:
  g_free(user);
}

static void spin_handle_chat_error(SpinData* spin,gchar* raw_room)
{
  gchar* room = NULL;
  if(!(room = spin_convert_user(spin,raw_room)))
    goto exit;


  if(g_hash_table_lookup(spin->pending_joins,room))
    {
      g_hash_table_remove(spin->pending_joins,room);
      
      GHashTable* table = g_hash_table_new(g_str_hash,g_str_equal);
      g_hash_table_insert(table,"room",room);
      purple_serv_got_join_chat_failed(spin->gc,table);
      g_hash_table_unref(table);
    }

 exit:
  g_free(room);
}

static void spin_handle_roominfo(SpinData* spin,gchar* rest)
{
  gchar *raw_room,*users,*raw_topic,*raw_hp,*room = NULL,*topic=NULL,*hp=NULL;
  spin_split_line(rest,&raw_room,&users,&raw_topic,&raw_hp,NULL);
  PurpleAccount* account = purple_connection_get_account(spin->gc);
  if(!(room = spin_convert_user(spin,raw_room)))
    goto exit;

  PurpleConversation* conv;
  conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,
					       room,account);
  if(!conv)
    {
      spin_chat_notfound(spin,room,raw_room);
      goto exit;
    }

  topic = spin_convert_in_text(raw_topic);
  hp = spin_convert_in_text(raw_hp);
  
  if(hp && *hp)
    {
      gchar* text = g_strdup_printf("%s (%s)",topic,hp);
      purple_conv_chat_set_topic(PURPLE_CONV_CHAT(conv),NULL,text);
      g_free(text);
    }
  else
      purple_conv_chat_set_topic(PURPLE_CONV_CHAT(conv),NULL,topic);

 exit:
  g_free(room);
  g_free(topic);
  g_free(hp);
}

static void spin_handle_usermode(SpinData* spin,gchar* rest)
{
  gchar *ty,*raw_room,*raw_user,*raw_user_2,*room=NULL,*user=NULL,*user_2=NULL,
    *formatted_msg = NULL,*dummy,*escaped_msg=NULL;
  spin_split_line(rest,&raw_room,&ty,&raw_user,&raw_user_2,&dummy,NULL);
  if( !ty
      || !(room = spin_convert_user(spin,raw_room))
      || !(user = spin_convert_user(spin,raw_user))
      || !(user_2 = spin_convert_user(spin,raw_user_2)))
    goto exit;

  PurpleAccount* account = purple_connection_get_account(spin->gc);
  PurpleConversation* conv =
    purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,room,account);
  if(!conv)
    {
      spin_chat_notfound(spin,room,raw_room);
      goto exit;
    }

  const gchar* msg = NULL;
  PurpleConvChatBuddyFlags flags =
    purple_conv_chat_user_get_flags(PURPLE_CONV_CHAT(conv),user);

  purple_debug_info("spin","flags was %u\n",flags);

  switch(*ty)
    {
    case 'a':
      flags |= PURPLE_CBFLAGS_HALFOP; 
      msg = _("%2$s gives operator rights to %1$s");
      break;
    case 'A':
      flags &= ~PURPLE_CBFLAGS_HALFOP; 
      msg = _("%2$s removes operator rights from %1$s");
      break;
    case 'b':
      flags |= PURPLE_CBFLAGS_VOICE; 
      msg = _("%2$s gives voice to %1$s");
      break;
    case 'B':
      flags &= ~PURPLE_CBFLAGS_VOICE; 
      msg = _("%2$s removes voice from %1$s");
      break;
    }

  purple_debug_info("spin","flags is %u\n",flags);

  purple_conv_chat_user_set_flags(PURPLE_CONV_CHAT(conv),user,flags);

  if(msg)
    {
      formatted_msg = g_strdup_printf(msg,user,user_2);
      escaped_msg = g_markup_escape_text(formatted_msg,-1);
      purple_conv_chat_write(PURPLE_CONV_CHAT(conv),user,escaped_msg,
			     PURPLE_MESSAGE_SYSTEM,
			     time(NULL));
    }

 exit:
  g_free(escaped_msg);
  g_free(formatted_msg);
  g_free(user);
  g_free(user_2);
  g_free(room);
}

void spin_handle_roommode(SpinData* spin,gchar* rest)
{
  gchar *ty,*args,*msg = NULL,*ip=NULL,*raw_user=NULL,*user=NULL,
    *raw_room,*room=NULL,*timecode;
  const gchar *msg_fmt;
  PurpleConversation* conv;
  PurpleAccount* account;
  GString* msg_str = NULL;

  account = purple_connection_get_account(spin->gc);

  spin_split_line(rest,&raw_room,&ty,&args,NULL);
  if(!ty
     || !(room = spin_convert_user(spin,raw_room)))
    return;

  conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,room,
					       account);
  if(!conv)
    {
      spin_chat_notfound(spin,room,raw_room);
      goto exit;
    }

  switch(*ty)
    {
    case '1':
      /* banned ip list */

      msg_str = g_string_new(_("Banned IP addresses:"));
      g_string_append(msg_str,"<ul>");
      
      while(args)
	{
	  spin_split_line(args,&raw_user,&timecode,&args,NULL);
	  if(!raw_user
	     || !(user = spin_convert_user(spin,raw_user)))
	    continue;

	  if(*user)
	    {
	      gchar* part;
	      part = g_markup_printf_escaped("<li>%s</li>",user);
	      g_string_append(msg_str,part);
	      g_free(part);
	    }

	  g_free(user);
	}
      user = NULL;

      g_string_append(msg_str,"</ul>");
      msg = g_string_free(msg_str,FALSE);
      break;
    case 'e': /* banned */
    case 'E': /* unbanned */
      if(*ty == 'e')
        msg_fmt = _("IP address %1$s has been banned by %2$s");
      else
	msg_fmt = _("IP address %1$s's ban has been removed by %2$s");
      spin_split_line(args,&raw_user,&ip,NULL);
      if(!(user = spin_convert_user(spin,raw_user)))
	goto exit;
      msg = g_markup_printf_escaped(msg_fmt,ip,user);
      break;
    case 'i':
      msg = g_strdup(_("The room locked for unregistered users"));
      break;
    }

  if(!msg)
    goto exit;

  purple_conv_chat_write(PURPLE_CONV_CHAT(conv),"",msg,
			 PURPLE_MESSAGE_SYSTEM,time(NULL));

 exit:
  g_free(msg);
  g_free(user);
  g_free(room);
}

void spin_parse_line(SpinData* spin,gchar* line)
{
#define HANDLE(CH,FUNC)						\
  case CH:							\
    FUNC(spin,line+1);						\
    break
  switch(line[0])
    {
      HANDLE('a',spin_handle_connected);
      HANDLE('e',spin_handle_disconnected);
      HANDLE('h',spin_handle_private_msg);
      HANDLE('=',spin_handle_status);
      HANDLE('>',spin_handle_notify);
      HANDLE('J',spin_handle_ping);
      HANDLE('l',spin_handle_list);
      HANDLE('+',spin_handle_joinleave);
      HANDLE('g',spin_handle_chat_msg);
      HANDLE('j',spin_handle_chatter_list);
      HANDLE('x',spin_handle_msg_error);
      HANDLE('v',spin_handle_chat_error);
      HANDLE('o',spin_handle_roominfo);
      HANDLE('|',spin_handle_usermode);
      HANDLE('n',spin_handle_roommode);
    default:
      purple_debug_info("spin","unrecognized line: %s\n",line);
    }
}

