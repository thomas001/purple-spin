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

#include "spin.h"
#include "cmds.h"
#include "spin_cmds.h"
#include "spin_privacy.h"
#include "spin_parse.h"

typedef void (*SpinCmdFunc)(PurpleConversation* conv,
			    SpinData* spin,const gchar** args,
			    gpointer userp,gchar** error);

typedef struct
{
  SpinCmdFunc func;
  gpointer userp;
  const gchar* fmt;
} SpinCmdInfo;

static PurpleCmdRet spin_handle_command(PurpleConversation* conv,
					  const gchar* cmd,
					  gchar** args,gchar** error,
					  void* data)
{
  SpinCmdInfo* i = (SpinCmdInfo*) data;
  PurpleAccount* acc = purple_conversation_get_account(conv);
  PurpleConnection* gc = purple_account_get_connection(acc);

  if(!gc)
    return PURPLE_CMD_RET_CONTINUE;

  SpinData* spin = (SpinData*) gc->proto_data;

  gchar** f_args = g_new0(gchar*,strlen(i->fmt)+1),
    **cur_f_arg = f_args,**cur_arg = args;
  const gchar* cur_fmt = i->fmt;

  while(*cur_fmt)
    {
      switch(*cur_fmt++)
	{
	case 'R': /* unencoded room argument */
	  if(strchr(*cur_arg,'#'))
	    {
	      *error = g_strdup(_("Room names must not contain #"));
	      goto arg_fail;
	    }
	case 'U': /* unencoded user name */
	  /* check for valid user name */
	  if(!(*cur_f_arg = spin_encode_user(*cur_arg)))
	    {
	      *error = g_strdup(_("Invalid character in room or user name"));
	      goto arg_fail;
	    }
	  g_free(*cur_f_arg);
	  *cur_f_arg++ = g_strstrip(g_strdup(*cur_arg++));
	  break;
	case 'r': /* encoded room argument */
	  if(strchr(*cur_arg,'#'))
	    {
	      *error = g_strdup(_("Room names must not contain #"));
	      goto arg_fail;
	    }
	case 'u': /* encoded user argument */
	  *cur_f_arg = g_strstrip(spin_encode_user(*cur_arg++));
	  if(!*cur_f_arg++)
	    {
	      *error = g_strdup(_("Invalid character in room or user name"));
	      goto arg_fail;
	    }
	  break;
	case 'n': /* encoded conversation name */
	  *cur_f_arg = spin_encode_user(purple_conversation_get_name(conv));
	  if(!*cur_f_arg++)
	    {
	      *error = g_strdup(_("Invalid conversation name, "
				  "something went wrong here..."));
	      goto arg_fail;
	    }
	  break;
	case 'N': /* unencoded conversation name */
	  *cur_f_arg++ = g_strdup(purple_conversation_get_name(conv));
	  break;
	default:
	  *cur_f_arg++ = g_strdup(*cur_arg++);
	}
    }

  i->func(conv,spin,(const gchar**) f_args,i->userp,error);

 arg_fail:
  g_strfreev(f_args);
  
  if(*error)
    return PURPLE_CMD_RET_FAILED;
  else
    return PURPLE_CMD_RET_OK;
}

static void spin_register_command(const gchar* args,PurpleCmdFlag flags,
				  gint priomod,
				  SpinCmdFunc f,const gchar* help,
				  gpointer userp,...)
{
  va_list ap;
  const gchar* name;

  va_start(ap,userp);

  SpinCmdInfo* info = g_new(SpinCmdInfo,1);
  info->func = f;
  info->userp = userp;
  info->fmt = g_strdup(args);

  gchar* new_args = g_new0(gchar,strlen(args)+1),*j;
  const gchar* i;
  for(i = args,j = new_args; *i; ++i)
    {
      if(strchr("uUw",*i))
	*j++ = 'w';
      if(strchr("rRs",*i))
	*j++ = 's';
    }

  while((name = va_arg(ap,const gchar*)))
    {
      purple_cmd_register(name,new_args,PURPLE_CMD_P_PRPL + priomod,
			  flags | PURPLE_CMD_FLAG_PRPL_ONLY,"prpl-spin",
			  spin_handle_command,help,info);
    }

  va_end(ap);

  g_free(new_args);
}

void spin_cmd_back(PurpleConversation* conv,
		   SpinData* spin,const gchar** args,
		   gpointer userp,gchar** error)
{
  purple_account_set_status(purple_conversation_get_account(conv),
			    "available",TRUE,NULL);
}

void spin_cmd_away(PurpleConversation* conv,
		   SpinData* spin,const gchar** args,
		   gpointer userp,gchar** error)
{
  purple_account_set_status(purple_conversation_get_account(conv),
			    "away",TRUE,"message",args[0],NULL);
}

void spin_cmd_ignore(PurpleConversation* conv,
		     SpinData* spin,const gchar** args,
		     gpointer userp,gchar** error)
{
  spin_ignore_user(spin,args[0]);
}

void spin_cmd_unignore(PurpleConversation* conv,
		     SpinData* spin,const gchar** args,
		       gpointer userp,gchar** error)
{
  spin_unignore_user(spin,args[0]);
}

void spin_cmd_join(PurpleConversation* conv,
		   SpinData* spin,const gchar** args,
		   gpointer userp,gchar** error)
{
  GHashTable* data = g_hash_table_new_full(g_str_hash,g_str_equal,NULL,g_free);
  g_hash_table_insert(data,"room",g_strdup(args[0]));
  serv_join_chat(spin->gc,data);
  g_hash_table_unref(data);
}

void spin_cmd_msg(PurpleConversation* conv,
		  SpinData* spin,const gchar** args,
		  gpointer userp,gchar** error)
{
  PurpleAccount* account = purple_connection_get_account(spin->gc);
  PurpleConversation* new_conv =
    purple_conversation_new(PURPLE_CONV_TYPE_IM,account,args[0]);
  purple_conversation_present(new_conv);
  
  if(args[1] && *args[1])
    serv_send_im(spin->gc,args[0],args[1],0);
}

void spin_cmd_mail(PurpleConversation* conv,
		   SpinData* spin,const gchar** args,
		   gpointer userp,gchar** error)
{
  gchar* user = g_uri_escape_string(args[0],NULL,FALSE);
  gchar* url = spin_session_url(spin,"/mail/create?user=%s",user);
  purple_notify_uri(spin->gc,url);
  g_free(url);
  g_free(user);
}

void spin_cmd_gift(PurpleConversation* conv,
		   SpinData* spin,const gchar** args,
		   gpointer userp,gchar** error)
{
  gchar* user = g_uri_escape_string(args[0],NULL,FALSE);
  gchar* url = spin_session_url(spin,"/gifts/index?user=%s",user);
  purple_notify_uri(spin->gc,url);
  g_free(url);
  g_free(user);
}

void spin_cmd_srvkick(PurpleConversation* conv,
		      SpinData* spin,const gchar** args,
		      gpointer userp,gchar** error)
{
  spin_write_command(spin,'1',args[0],NULL);
}

void spin_cmd_srvkickban(PurpleConversation* conv,
			 SpinData* spin,const gchar** args,
			 gpointer userp,gchar** error)
{
  spin_write_command(spin,'2',"c",args[0],NULL);
  spin_write_command(spin,'2',"a",args[0],NULL);
  spin_write_command(spin,'1',args[0],NULL);
}

void spin_cmd_srvmute(PurpleConversation* conv,
		      SpinData* spin,const gchar** args,
		      gpointer userp,gchar** error)
{
  spin_write_command(spin,'E',args[0],NULL);
}


void spin_cmd_banlist(PurpleConversation* conv,
		      SpinData* spin,const gchar** args,
		      gpointer userp,gchar** error)
{
  spin_write_command(spin,'t',args[0],"1","",NULL);
}

void spin_cmd_unreglock(PurpleConversation* conv,
			 SpinData* spin,const gchar** args,
			gpointer userp,gchar** error)
{
  spin_write_command(spin,'t',args[0],"i","",NULL);
}

void spin_cmd_room_flags(PurpleConversation* conv,
			 SpinData* spin,const gchar** args,
			 gpointer userp,gchar** error)
{
  spin_write_command(spin,'|',args[0],userp,args[1],"",NULL);
}

void spin_cmd_kick(PurpleConversation* conv,
			 SpinData* spin,const gchar** args,
			 gpointer userp,gchar** error)
{
  spin_write_command(spin,'f',args[1],args[0],NULL);
}

void spin_cmd_unban(PurpleConversation* conv,
			 SpinData* spin,const gchar** args,
			 gpointer userp,gchar** error)
{
  spin_write_command(spin,'t',args[0],"E",args[1],NULL);
}

void spin_cmd_kickban(PurpleConversation* conv,
			 SpinData* spin,const gchar** args,
			 gpointer userp,gchar** error)
{
  spin_write_command(spin,'t',args[0],"e","a",args[1],NULL);
  spin_write_command(spin,'f',args[1],args[0],NULL);
}

void spin_cmd_warn(PurpleConversation* conv,
			 SpinData* spin,const gchar** args,
			 gpointer userp,gchar** error)
{
  spin_write_command(spin,'g',args[0],"0","warn",args[1],args[2],
		     NULL);
}

void spin_cmd_ban(PurpleConversation* conv,
			 SpinData* spin,const gchar** args,
			 gpointer userp,gchar** error)
{
  static GRegex* valid_ban_re = NULL;
  if(!valid_ban_re)
    {
      GError* error = NULL;
      valid_ban_re = g_regex_new("^\\d+\\.\\d+\\.\\d+(?:\\.(?:\\d+|\\*))$",
				 G_REGEX_OPTIMIZE,0,&error);
      g_assert(error == NULL);
    }
  
  if(!g_regex_match(valid_ban_re,args[0],0,NULL))
    {
      *error = g_strdup(_("Invalid ban expression"));
      return;
    }

  const gchar* room = purple_conversation_get_name(conv);
  gchar* encoded_room = spin_encode_user(room);
  g_return_if_fail(encoded_room);

  spin_write_command(spin,'t',encoded_room,"e","0",args[0],NULL);

  g_free(encoded_room);
}

typedef struct
{
  const gchar* ty;
  const gchar* fmt;
} EmoteInfo;

void spin_cmd_emote(PurpleConversation* conv,
		    SpinData* spin,const gchar** args,
		    gpointer userp,gchar** error)
{
  EmoteInfo* info = (EmoteInfo*) userp;
  gchar* encoded_conv_name =
    spin_encode_user(purple_conversation_get_name(conv));
  g_return_if_fail(encoded_conv_name);
  
  gchar* send_text = g_strdup_printf(info->fmt,args[0]);
  gchar* out_text = NULL;

  if(purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_IM)
    {
      PurpleAccount* account = purple_connection_get_account(spin->gc);
      const gchar* user = purple_account_get_username(account);
      out_text = spin_write_chat(*info->ty,user,send_text);

      spin_write_command(spin,'h',encoded_conv_name,"0",info->ty,send_text,
			 NULL);
      purple_conv_im_write(PURPLE_CONV_IM(conv),
			   purple_connection_get_display_name(spin->gc),
			   out_text,PURPLE_MESSAGE_SEND,time(NULL));
    }
  else if(purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT)
    {
      const gchar* user = purple_conv_chat_get_nick(PURPLE_CONV_CHAT(conv));
      out_text = spin_write_chat(*info->ty,user,send_text);
      
      spin_write_command(spin,'g',encoded_conv_name,info->ty,send_text,NULL);
      /* serv_got_chat_in(spin->gc,purple_conv_chat_get_id(PURPLE_CONV_CHAT(conv)), */
      /* 		       purple_conv_chat_get_nick(PURPLE_CONV_CHAT(conv)), */
      /* 		       PURPLE_MESSAGE_SEND,out_text,time(NULL)); */
    }

  g_free(send_text);
  g_free(out_text);
}

void spin_register_commands()
{
  spin_register_command("",PURPLE_CMD_FLAG_IM|PURPLE_CMD_FLAG_CHAT,1,
			spin_cmd_back,
			_("/(away|back): Marks you as being back"),NULL,
			"back","away",NULL);
  spin_register_command("s",PURPLE_CMD_FLAG_IM|PURPLE_CMD_FLAG_CHAT,0,
			spin_cmd_away,
			_("/(away|afk) REASON: Marks you as being away"),NULL,
			"away","afk",NULL);

  spin_register_command("w",PURPLE_CMD_FLAG_IM|PURPLE_CMD_FLAG_CHAT,0,
			spin_cmd_ignore,
			_("/(ignore|ig|block) USER: Ignores the given user"),
			NULL,"ignore","ig","block",NULL);
  spin_register_command("w",PURPLE_CMD_FLAG_IM|PURPLE_CMD_FLAG_CHAT,0,
			spin_cmd_unignore,
			_("/(unignore|unig|unblock) USER: "
			  "Unignores the given user"),NULL,
			"unignore","unig","unblock",NULL);

  spin_register_command("R",PURPLE_CMD_FLAG_IM|PURPLE_CMD_FLAG_CHAT,0,
			spin_cmd_join,_("/join ROOMNAME: Joins the given room"),
			NULL,"join",NULL);

  spin_register_command("Us",PURPLE_CMD_FLAG_IM|PURPLE_CMD_FLAG_CHAT,0,
			spin_cmd_msg,
			_("/msg USER TEXT: Messages the user"),NULL,
			"msg",NULL);
  spin_register_command("U",PURPLE_CMD_FLAG_IM|PURPLE_CMD_FLAG_CHAT,0,
			spin_cmd_msg,
			_("/msg USER: Opens a message window to the user"),NULL,
			"msg",NULL);

  spin_register_command("U",PURPLE_CMD_FLAG_IM|PURPLE_CMD_FLAG_CHAT,0,
			spin_cmd_mail,
			_("/mail USER: Opens a new mail to the user"),NULL,
			"mail",NULL);
  spin_register_command("N",PURPLE_CMD_FLAG_IM|PURPLE_CMD_FLAG_IM,0,
			spin_cmd_mail,
			_("/mail: Opens a new mail to the conversion partner"),
			NULL,
			"mail",NULL);
  spin_register_command("U",PURPLE_CMD_FLAG_IM|PURPLE_CMD_FLAG_CHAT,0,
			spin_cmd_gift,
			_("/gift USER: Opens a new gift for the user"),NULL,
			"gift",NULL);
  spin_register_command("N",PURPLE_CMD_FLAG_IM|PURPLE_CMD_FLAG_IM,0,
			spin_cmd_gift,
			_("/gift: Opens a new gift for the conversion partner"),
			NULL,
			"gift",NULL);

  spin_register_command("u",PURPLE_CMD_FLAG_IM|PURPLE_CMD_FLAG_CHAT,0,
			spin_cmd_srvkick,
			_("/(servkick|sk) USER: Server kick"),NULL,
			"srvkick","sk",NULL);
  spin_register_command("u",PURPLE_CMD_FLAG_IM|PURPLE_CMD_FLAG_CHAT,0,
			spin_cmd_srvkick,
			_("/(srvkickban|skb) USER: Server kick+ban"),NULL,
			"srvkickban","skb",NULL);
  spin_register_command("u",PURPLE_CMD_FLAG_IM|PURPLE_CMD_FLAG_CHAT,0,
			spin_cmd_srvmute,
			_("/(srvmute|sm) USER: Server mute"),NULL,
			"srvmute","sm",NULL);
  
  spin_register_command("n",PURPLE_CMD_FLAG_CHAT,0,
			spin_cmd_banlist,
			_("/banlist: Receive the ban list"),NULL,
			"banlist",NULL);
  spin_register_command("n",PURPLE_CMD_FLAG_CHAT,0,
			spin_cmd_banlist,_("/unreglock: ???"),NULL,
			"unreglock",NULL);

  spin_register_command("nu",PURPLE_CMD_FLAG_CHAT,0,spin_cmd_room_flags,
			  _("/op USER: Gives OP to the user"),"a",
			  "op",NULL);
  spin_register_command("nu",PURPLE_CMD_FLAG_CHAT,0,spin_cmd_room_flags,
			  _("/deop USER: Removes OP from the user"),"A",
			  "deop",NULL);
  spin_register_command("nu",PURPLE_CMD_FLAG_CHAT,0,spin_cmd_room_flags,
			  _("/mute USER: Mutes the user"),"B",
			  "mute",NULL);
  spin_register_command("nu",PURPLE_CMD_FLAG_CHAT,0,spin_cmd_room_flags,
			  _("/unmute USER: Unmutes the user"),"b",
			  "unmute",NULL);

  spin_register_command("nu",PURPLE_CMD_FLAG_CHAT,0,
			spin_cmd_kick,_("/(kick|k) USER: Kicks a user"),NULL,
			"kick","k",NULL);
  spin_register_command("nu",PURPLE_CMD_FLAG_CHAT,0,
			spin_cmd_kickban,
			_("/(kickban|kb) USER: Kicks+bans a user"),NULL,
			"kickban","kb",NULL);
  spin_register_command("nu",PURPLE_CMD_FLAG_CHAT,0,
			spin_cmd_unban,
			_("/(unban|deban) USER: Unbans a user"),NULL,
			"unban","deban",NULL);
  spin_register_command("ns",PURPLE_CMD_FLAG_CHAT,0,
			spin_cmd_ban,_("/ban IP: Bans a IP address"),NULL,
			"ban",NULL);

  spin_register_command("nus",PURPLE_CMD_FLAG_CHAT,0,
			spin_cmd_warn,_("/warn USER REASON: Warns a user"),NULL,
			"warn",NULL);


#define SPIN_REGISTER_EMOTE(NAME,TY,FMT)				\
  {									\
    static EmoteInfo info = { (TY), (FMT) };				\
    gchar* help = g_strdup_printf("/%s TEXT: %s",(NAME),		\
				  _("Executes the emote command"));	\
    spin_register_command("s",PURPLE_CMD_FLAG_CHAT|PURPLE_CMD_FLAG_IM,0, \
			  spin_cmd_emote,help,&info, (NAME),NULL);	\
    g_free(help);							\
  }

  SPIN_REGISTER_EMOTE("me","c","%s");
  SPIN_REGISTER_EMOTE("mes","c","s %s");
  SPIN_REGISTER_EMOTE("me's","c","'s %s");
  SPIN_REGISTER_EMOTE("me'","c","' %s");
  SPIN_REGISTER_EMOTE("think","c",".oO( %s )");
  SPIN_REGISTER_EMOTE("th","c",".oO( %s )");
  SPIN_REGISTER_EMOTE("ooc","c","[%s]");
  SPIN_REGISTER_EMOTE("echo","e","%s");
}
