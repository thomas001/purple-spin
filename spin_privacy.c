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

#include "spin_privacy.h"
#include "spin_web.h"
#include "spin_login.h"

#include "debug.h"
#include "privacy.h"

static void two_arg_free(gpointer p,gpointer dummy)
{
  g_free(p);
}

static void spin_sync_privacy_cb(PurpleUtilFetchUrlData* url_data,
				 gpointer userp,JsonNode* node,
				 const gchar* error_message)
{
  PurpleConnection* gc = (PurpleConnection*) userp;
  PurpleAccount* account;
  GSList* blocks = NULL;
  SpinData* spin;

  if(!PURPLE_CONNECTION_IS_VALID(gc))
    return;

  spin = (SpinData*) gc->proto_data;
  account = purple_connection_get_account(gc);

  if(!node)
    {
      gchar* err_text = g_strdup_printf(_("Could not receive block list: %s"),
					error_message);
      purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				     err_text);
      g_free(err_text);
      return;
    }

  if(JSON_NODE_TYPE(node) == JSON_NODE_OBJECT)
    {
      JsonObject* object = json_node_get_object(node);
      GList* members;
      
      for(members = json_object_get_members(object); members;
	  members = g_list_next(members))
	{
	  const gchar *name = (const gchar*) members->data;
	  gchar* normalized = g_strdup(purple_normalize(account,name));
	  blocks = g_slist_append(blocks,normalized);
	}

    }
  else if(JSON_NODE_TYPE(node) == JSON_NODE_ARRAY)
    {
      JsonArray* array = json_node_get_array(node);
      guint i;

      for(i = 0; i < json_array_get_length(array); ++i)
	{
	  JsonNode* elem = json_array_get_element(array,i);
	  if(JSON_NODE_TYPE(elem) != JSON_NODE_VALUE)
	    {
	      purple_debug_info("spin","invalid block list entry type: %i\n",
				JSON_NODE_TYPE(elem));
	      continue;
	    }

	  const gchar* name = json_node_get_string(elem);
	  if(!name)
	    continue;

	  gchar* normalized = g_strdup(purple_normalize(account,name));
	  blocks = g_slist_append(blocks,normalized);
	}
    }
  else
    {
      purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				     _("Invalid block list format received"));
    }

  /* g_slist_foreach(account->permit,two_arg_free,NULL); */
  /* g_slist_free(account->permit); */
  /* account->permit = NULL; */

  g_slist_foreach(account->deny,two_arg_free,NULL);
  g_slist_free(account->deny);
  account->deny = blocks;

  spin_connect_add_state(spin,SPIN_STATE_GOT_INITIAL_BLOCK_LIST);
}

void spin_sync_privacy_lists(SpinData* spin)
{

  spin_fetch_json_request(spin,"http://www.spin.de/api/blocks",
			  spin_sync_privacy_cb,spin->gc,
			  "session",spin->session,
			  "utf8","1",
			  "dia","1",
			  NULL);
}

static void spin_privacy_policy_cb(PurpleUtilFetchUrlData* url_data,
				   gpointer userp,
				   const gchar* data,gsize datalen,
				   const gchar* error_msg)
{
  PurpleConnection* gc = (PurpleConnection*) userp;

  if(!PURPLE_CONNECTION_IS_VALID(gc))
    return;
  
  if(!data)
    {
      gchar* err_text = g_strdup_printf(_("Could not set privacy policy: %s"),
					error_msg);
      purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				     err_text);
      g_free(err_text);
    }
}

void spin_sync_privacy_policy(SpinData* spin)
{
  PurpleAccount* account = purple_connection_get_account(spin->gc);
  const gchar* spin_mode = NULL;

  switch(account->perm_deny)
    {
    case PURPLE_PRIVACY_ALLOW_ALL: /* unsupported */ break;
    case PURPLE_PRIVACY_DENY_ALL: spin_mode = "1"; break;
    case PURPLE_PRIVACY_ALLOW_USERS: /* unsupported */ break;
    case PURPLE_PRIVACY_DENY_USERS: spin_mode = "0"; break;
    case PURPLE_PRIVACY_ALLOW_BUDDYLIST: spin_mode = "2"; break;
    }

  if(!spin_mode)
    {
      /* TODO: i think this will only annoy users */

      purple_notify_warning(spin->gc,_("Unsupported privacy mode"),
      			    _("The selected privacy mode is not supported by"
      			      " spin.de"),
      			    _("The privacy mode will not be set server side"));
      return;
    }

  spin_fetch_post_request(spin,"http://www.spin.de/prefs/index",
			  spin_privacy_policy_cb,spin->gc,
			  "session_id",spin->session,
			  "dialog",spin_mode,
			  /* "mail", "0"/"1", */
			  /* "adfilter", "0"/"1", */
			  /* "onlinestatus", "0"/"1", */
			  /* "searchengine", "0"/"1", */
			  /* "targetads", "0"/"1", */
			  /* "analytics", "0"/"1", */
			  NULL);
}

void spin_set_permit_deny(PurpleConnection* gc)
{
  g_return_if_fail(gc->proto_data);
  
  SpinData* spin = (SpinData*) gc->proto_data;

  spin_sync_privacy_policy(spin);
}

void spin_add_permit(PurpleConnection* gc,const gchar* name)
{
  /* TODO: annoying.... */
  purple_notify_warning(gc,_("Permit lists not supported by spin.de"),
  			_("There is no server side permit list, so changes "
  			  "won't be visible outside this pidgin"),
  			NULL);
}

void spin_rem_permit(PurpleConnection* gc,const gchar* name)
{
  spin_add_permit(gc,name);
}

void spin_ignore_user(SpinData* spin,const gchar* name)
{
  g_return_if_fail(spin);
  g_return_if_fail(name);

  gchar* encoded_user = spin_encode_user(name);
  if(!encoded_user)
    return;

  spin_write_command(spin,'w',encoded_user,NULL);

  g_free(encoded_user);
}

void spin_unignore_user(SpinData* spin,const gchar* name)
{
  g_return_if_fail(spin);
  g_return_if_fail(name);
  
  gchar* encoded_user = spin_encode_user(name);
  PurpleAccount* account = purple_connection_get_account(spin->gc);
  GList* chats;
  if(!encoded_user)
    return;

  spin_write_command(spin,'O',encoded_user,NULL);

  /* the javascript code refetches chatter lists,so we do this too */
  for(chats = purple_get_chats(); chats; chats = g_list_next(chats))
    {
      PurpleConversation* conv = (PurpleConversation*) chats->data;
      if(purple_conversation_get_account(conv) != account)
	continue;

      gchar* encoded_room =
	spin_encode_user(purple_conversation_get_name(conv));
      purple_conv_chat_clear_users(PURPLE_CONV_CHAT(conv));
      spin_write_command(spin,'j',encoded_room,NULL);
      g_free(encoded_room);
    }

  g_free(encoded_user);
}

void spin_add_deny(PurpleConnection* gc,const gchar* name)
{
  // TODO
}

void spin_rem_deny(PurpleConnection* gc,const gchar* name)
{
  // TODO
}
