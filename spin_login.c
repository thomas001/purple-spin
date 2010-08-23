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

#include "spin_login.h"
#include "spin_web.h"
#include "debug.h"
#include <unistd.h>
#ifdef WIN32
#  include <winsock2.h>
#else
#  include <sys/socket.h>
#endif

static void connect_cb(void* data,gint source,const char* errmsg)
{
  PurpleConnection* gc = (PurpleConnection*)data;

  if(!PURPLE_CONNECTION_IS_VALID(gc))
    {
      if(source >= 0)
	close(source);
      return;
    }

  if(source < 0)
    {
      purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				     errmsg);
      return;
    }

  gc->flags =  /*PURPLE_CONNECTION_HTML |*/  PURPLE_CONNECTION_NO_BGCOLOR
    | PURPLE_CONNECTION_NO_NEWLINES | PURPLE_CONNECTION_NO_FONTSIZE 
    | PURPLE_CONNECTION_NO_URLDESC | PURPLE_CONNECTION_NO_IMAGES;

  SpinData* spin = (SpinData*)gc->proto_data;
  spin->fd = source;

  spin_start_read(spin);

  spin_write_command(spin,'A',"prpl-spin",NULL);
  spin_write_command(spin,'B',"I'm a bot.",NULL);
  spin_write_command(spin,'a',spin->username,spin->session,NULL);
}

static void spin_do_chat_login(SpinData* spin)
{
  PurpleAccount* account = purple_connection_get_account(spin->gc);
  /* gchar** userparts = g_strsplit(purple_account_get_username(account),"#",0); */
  /* /\* gchar* username = userparts[0]; *\/ */
  /* gchar* host = userparts[1]; */
  /* gint port = atoi(userparts[2]); */
  const gchar* host = purple_account_get_string(account,"server","www.spin.de");
  gint port = purple_account_get_int(account,"port",3003);

  if(!purple_proxy_connect(spin->gc, account, host, port, connect_cb, spin->gc))
    {
      purple_connection_error_reason(spin->gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
  				     _("could not create socket"));
    }

  /* g_strfreev(userparts); */
}

static void spin_weblogin_cb(PurpleUtilFetchUrlData* url_data,gpointer userp,
			     JsonNode* node,const gchar* error_message)
{
  PurpleConnection* gc = (PurpleConnection*) userp;
  
  if(!PURPLE_CONNECTION_IS_VALID(gc))
    return;

  SpinData* spin = (SpinData*) gc->proto_data;
  PurpleAccount* account = purple_connection_get_account(gc);
  JsonObject* obj;
  if(!node)
    {
      purple_debug_error("spin","could not get web login: %s\n",error_message);
      purple_connection_error_reason
	(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
	 _("unable to get login reply from web server"));
      return;
    }

  if(JSON_NODE_TYPE(node) != JSON_NODE_OBJECT)
    {
      purple_connection_error_reason
	(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
	 _("invalid json format received"));
      return;
    }
  obj = json_node_get_object(node);

  JsonNode* status = json_object_get_member(obj,"status");
  if(!status || JSON_NODE_TYPE(status) != JSON_NODE_VALUE)
    {
      purple_connection_error_reason
	(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
	 _("invalid json format received"));
      return;
    }
    
      
  if(!g_str_has_prefix(json_node_get_string(status),"OK "))
    {
      purple_connection_error_reason
	(gc,PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
	 json_node_get_string(status));
      return;
    }

  JsonNode* session = json_object_get_member(obj,"session");
  if(!session || JSON_NODE_TYPE(session) != JSON_NODE_VALUE)
    {
      purple_connection_error_reason
	(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
	 _("no session found in json"));
      return;
    }
  spin->session = json_node_dup_string(session);

  
  JsonNode* login = json_object_get_member(obj,"username");
  if(!login || JSON_NODE_TYPE(login) != JSON_NODE_VALUE)
    {
      purple_connection_error_reason
	(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
	 _("no username found in json"));
      return;
    }
  spin->username = json_node_dup_string(login);
  spin->normalized_username =
    g_strdup(purple_normalize(account,json_node_get_string(login)));
  purple_connection_set_display_name(gc, spin->username);

  gchar* escaped_username = g_regex_escape_string(spin->username,-1);
  gchar* nick_regex_str = g_strdup_printf("(?i)\\b%ss?\\b",escaped_username);
  spin->nick_regex = g_regex_new(nick_regex_str,G_REGEX_OPTIMIZE,0,NULL);
  g_assert(spin->nick_regex);
  g_free(escaped_username);
  g_free(nick_regex_str);

  spin_connect_add_state(spin,SPIN_STATE_GOT_WEB_LOGIN);

  purple_connection_update_progress(gc,Q_("Progress|Chat login"),2,3);
  spin_do_chat_login(spin);
}

void spin_login(PurpleAccount* a)
{
  PurpleConnection* gc = purple_account_get_connection(a);
  /* gchar** userparts = g_strsplit(purple_account_get_username(a),"#",0); */
  /* if(!userparts[0] || !userparts[1] || !userparts[2]) */
  /*   { */
  /*     purple_connection_error_reason */
  /* 	(gc,PURPLE_CONNECTION_ERROR_INVALID_SETTINGS, */
  /* 	 "Misformated username string."); */
  /*     goto exit; */
  /*   } */

  /* gchar* username = userparts[0]; */
  /* gchar* host = userparts[1]; */
  /* gchar* port_str = userparts[2]; */
  /* gint port = atoi(port_str); */
  const gchar* username = purple_account_get_username(a);
  const gchar* host = purple_account_get_string(a,"server","www.spin.de");
  gint port = purple_account_get_int(a,"port",3003);

  /* if(userparts[3]) */
  /*   { */
  /*     purple_connection_error_reason */
  /* 	(gc,PURPLE_CONNECTION_ERROR_INVALID_SETTINGS, */
  /* 	 "Username must not contain '#'"); */
  /*     goto exit; */
  /*   } */

  if(!port || !host || !*host)
    {
      purple_connection_error_reason
	(gc,PURPLE_CONNECTION_ERROR_INVALID_SETTINGS,
	 _("invalid host or port"));
      goto exit;
    }

  GRegex* nick_regex = NULL;
  const gchar* nick_regex_str = purple_account_get_string(a,"nick-regex","");
  if(nick_regex_str[0])
    {
      GError* error = NULL;
      nick_regex = g_regex_new(nick_regex_str,G_REGEX_OPTIMIZE,0,&error);
      if(error)
	{
	  gchar* msg = g_strdup_printf(_("error compiling nick regex: %s"),
				       error->message);
	  purple_connection_error_reason
	    (gc,PURPLE_CONNECTION_ERROR_INVALID_SETTINGS,msg);
	  g_free(msg);
	  g_error_free(error);
	  goto exit;
	}
    }

  SpinData* spin;
  gc->proto_data = spin = g_new0(SpinData,1);
  spin->gc = gc;
  spin->inbuf = g_string_new("");
  spin->outbuf = purple_circ_buffer_new(0);
  spin->session = NULL;
  spin->state = 0;
  spin->nick_regex = nick_regex;
  spin->pending_joins = g_hash_table_new_full(g_str_hash,g_str_equal,
					      g_free,NULL);
  spin->updated_status_list = g_hash_table_new_full(g_str_hash,g_str_equal,
						    g_free,NULL);

  purple_connection_set_state(gc, PURPLE_CONNECTING);
  purple_connection_update_progress(gc,Q_("Progress|Web login"),1,4);

  gboolean secure_login = purple_account_get_bool(a,"secure-login",TRUE);
  const gchar *normal_url = "http://www.spin.de/api/login",
    *secure_url = "https://www.spin.de/api/login",
    *url = secure_login ? secure_url : normal_url;

  gchar* encoded_username = spin_encode_user(username);
  if(!encoded_username)
    {
      purple_connection_error_reason
	(gc,PURPLE_CONNECTION_ERROR_INVALID_SETTINGS,
	 _("Invalid characters in username"));
      goto exit;
    }      

  spin_fetch_json_request(spin,url,
			  spin_weblogin_cb,gc,
			  "user",encoded_username,
			  "password",purple_account_get_password(a),
			  /* "server",port_str, */
			  NULL);

  g_free(encoded_username);

 exit:;
  /* g_strfreev(userparts); */
}

void spin_close(PurpleConnection* gc)
{
  purple_debug_info("spin","close\n");
  SpinData* spin = (SpinData*) gc->proto_data;
  g_return_if_fail(spin);

  if(spin->ping_timeout_handle)
    purple_timeout_remove(spin->ping_timeout_handle);
  if(spin->read_handle)
    purple_input_remove(spin->read_handle);
  if(spin->write_handle)
    purple_input_remove(spin->write_handle);
  if(spin->inbuf)
    g_string_free(spin->inbuf,TRUE);
  if(spin->outbuf)
    purple_circ_buffer_destroy(spin->outbuf);
  if(spin->fd)
    {
      send(spin->fd,"e\n",2, 0 
#if HAVE_MSG_DONTWAIT
	   | MSG_DONTWAIT
#endif
#if HAVE_MSG_NOSIGNAL
	   | MSG_NOSIGNAL
#endif
	   );
      close(spin->fd);
    }
  if(spin->session)
    g_free(spin->session);
  if(spin->pending_joins)
    g_hash_table_destroy(spin->pending_joins);
  if(spin->updated_status_list)
    g_hash_table_destroy(spin->updated_status_list);
  if(spin->username)
    g_free(spin->username);
  if(spin->normalized_username)
    g_free(spin->normalized_username);
  if(spin->nick_regex)
    g_regex_unref(spin->nick_regex);

  g_free(spin);
  gc->proto_data = NULL;
}
      
void spin_connect_add_state(SpinData* spin,SpinConnectionState state)
{
  g_return_if_fail(spin);

  spin->state |= state;
  if((spin->state == SPIN_STATE_ALL_CONNECTION_STATES)
     && (purple_connection_get_state(spin->gc) == PURPLE_CONNECTING))
    {
      purple_connection_set_state(spin->gc,PURPLE_CONNECTED);
    }
}
