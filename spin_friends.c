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

#include "spin_friends.h"
#include "spin_web.h"
#include "spin_notify.h"
#include "debug.h"
#include "spin_login.h"

static PurpleBuddy* spin_find_buddy_in_list(GSList* buddies,const gchar* id)
{
  GSList* i;
  for(i = buddies; i; i = i->next)
    {
      PurpleBuddy* buddy = (PurpleBuddy*) i->data;
      const gchar* buddy_id =
	purple_blist_node_get_string(&buddy->node,"spin-id");
      if(g_strcmp0(id,buddy_id) == 0)
	return buddy;
    }
  return NULL;
}

PurpleBuddy* spin_find_buddy_by_id(PurpleAccount* account,const gchar* id)
{
  GSList* account_buddies = purple_find_buddies(account,NULL);
  PurpleBuddy* ret = spin_find_buddy_in_list(account_buddies,id);
  g_slist_free(account_buddies);
  return ret;
}

/* static void fetch_photo_cb(PurpleConnection* gc,CURLcode code,CURL* easy, */
/* 			   GByteArray* data,gpointer p) */
/* { */
/*   if(!PURPLE_CONNECTION_IS_VALID(gc)) */
/*     goto exit; */

/*   glong response; */
/*   curl_easy_getinfo(easy,CURLINFO_RESPONSE_CODE,&response); */
/*   if(code != CURLE_OK || response!=200) */
/*     goto exit; */
  
/*   PurpleAccount* account = purple_connection_get_account(gc); */
/*   gchar* user = (gchar*) p; */
/*   gchar* url; */
/*   curl_easy_getinfo(easy,CURLINFO_EFFECTIVE_URL,&url); */
/*   purple_buddy_icons_set_for_user(account,user,data->data,data->len,url); */

/*  exit: */
/*   g_free(p); */
/*   curl_easy_cleanup(easy); */
/* } */

typedef struct FetchPhotoData_
{
  PurpleConnection* gc;
  gchar* url;
  gchar* user;
} FetchPhotoData;

static void fetch_photo_cb(PurpleUtilFetchUrlData* url_data,gpointer user,
			   const gchar* url_text,gsize len,const gchar* error_message)
{
  FetchPhotoData* f = (FetchPhotoData*) user;
  if(!PURPLE_CONNECTION_IS_VALID(f->gc))
    goto exit;

  PurpleAccount* account = purple_connection_get_account(f->gc);
  gpointer copied = g_memdup(url_text,len);
  purple_buddy_icons_set_for_user(account,f->user,copied,
				  len,f->url);

 exit:
  g_free(f->user);
  g_free(f->url);
  g_free(f);
}

static void spin_sync_photo(SpinData* spin,PurpleBuddy* buddy,const gchar* url)
{
  PurpleAccount* account = purple_connection_get_account(spin->gc);
  const gchar* old_url = purple_buddy_icons_get_checksum_for_user(buddy);
  if(g_strcmp0(url,old_url) == 0)
    return;

  if(!url)
    purple_buddy_icons_set_for_user(account,purple_buddy_get_name(buddy),
				    NULL,0,NULL);
  else
    {
      FetchPhotoData* user = g_new(FetchPhotoData,1);
      user->url = g_strdup(url);
      user->user = g_strdup(purple_buddy_get_name(buddy));
      user->gc = spin->gc;
      spin_fetch_url_request(spin,url,fetch_photo_cb,user);
    }

}

static PurpleBuddy* spin_sync_buddy(SpinData* spin,
				    GSList* buddies,const gchar* id,
				    const gchar* name,guint online,
				    const gchar* away,const gchar* photo)
{
  PurpleAccount* account = purple_connection_get_account(spin->gc);
  /* gchar* lower_name = g_utf8_strdown(name,-1); */
  PurpleBuddy* buddy = spin_find_buddy_in_list(buddies,id);
  if(!buddy)
    {
      purple_debug_info("spin","adding buddy: %s\n",/*lower_*/name);
      buddy = purple_buddy_new(account,/*lower_*/name,NULL);
      purple_blist_add_buddy(buddy,NULL,NULL,NULL);
      purple_blist_node_set_string(&buddy->node,"spin-id",id);
    }
  /* purple_normalize here? */
  if(g_strcmp0(purple_buddy_get_name(buddy),name) != 0)
    {
      spin_notify_nick_changed(spin,purple_buddy_get_name(buddy),name);
      purple_blist_rename_buddy(buddy,name);
    }

  spin_sync_photo(spin,buddy,photo);

  /* do not set status if we got a status after the HTTP request */
  if(g_hash_table_lookup(spin->updated_status_list,
			 purple_normalize(account,name)))
    return buddy;

  if(online && *away)
    purple_prpl_got_user_status(account,/*lower_*/name,"away",
				"message",away,NULL);
  else if(online)
    purple_prpl_got_user_status(account,/*lower_*/name,"available",NULL);
  else
    purple_prpl_got_user_status(account,/*lower_*/name,"offline",NULL);

  return buddy;
  /* g_free(lower_name); */
}

static void spin_receive_friends_cb(PurpleUtilFetchUrlData* url_data,
				    gpointer userp,
				    JsonNode* node,
				    const gchar* error_message)
{
  PurpleConnection* gc = (PurpleConnection*) userp;
  
  if(!PURPLE_CONNECTION_IS_VALID(gc))
    return;

  if(!node)
    {
      purple_debug_error("spin","friend list error:%s\n",error_message);
      purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				     _("could not receive friend list"));
      return;
    }
  
  SpinData* spin = (SpinData*) gc->proto_data;
  PurpleAccount* account = purple_connection_get_account(gc);
  GHashTable* found_buddies = g_hash_table_new(g_direct_hash,g_direct_equal);
  GSList* account_buddies = purple_find_buddies(account,NULL);
  
  if(!node || JSON_NODE_TYPE(node) != JSON_NODE_ARRAY)
    {
      purple_connection_error_reason
	(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
	 _("invalid friend list format"));
      goto exit;
    }
  JsonArray* friends = json_node_get_array(node);
  guint i;
  for(i = 0; i < json_array_get_length(friends); ++i)
    {
      node = json_array_get_element(friends,i);
      JsonArray* entry;
      if(JSON_NODE_TYPE(node) != JSON_NODE_ARRAY
	 || json_array_get_length(entry = json_node_get_array(node)) != 7)
	{
	  purple_debug_info("spin","invalid friend list entry\n");
	  continue;
	}

      const gchar* id = json_node_get_string(json_array_get_element(entry,0));
      const gchar* name = json_node_get_string(json_array_get_element(entry,1));
      guint online = json_node_get_int(json_array_get_element(entry,2));
      const gchar* away = json_node_get_string(json_array_get_element(entry,3));
      const gchar* photo =json_node_get_string(json_array_get_element(entry,5));
      purple_debug_info("spin","got friend info: %s %s %i %s %s\n",
			id,name,online,	away,photo);
      if(!name || !away || !photo || !id)
	continue;

      PurpleBuddy* buddy = spin_sync_buddy(spin,account_buddies,id,name,
					   online,away,photo);
      g_hash_table_insert(found_buddies,buddy,(gpointer)0x1);
    }

  GSList* b;
  for(b = account_buddies; b; b = b->next)
    {
      if(!g_hash_table_lookup(found_buddies,b->data))
	{
	  spin_notify_nick_removed
	    (spin,purple_buddy_get_name((PurpleBuddy*) b->data));
	  purple_blist_remove_buddy((PurpleBuddy*)b->data);
	}
    }

  spin_connect_add_state(spin,SPIN_STATE_GOT_INITIAL_FRIEND_LIST);

 exit:
  g_slist_free(account_buddies);
  g_hash_table_destroy(found_buddies);
}

void spin_receive_friends(SpinData* spin)
{
  g_return_if_fail(spin);
  g_return_if_fail(spin->session);

  /* this should not be neccessay if we know that we are actually in sync
     with the real status of all buddys.
     but it should not hurt,too.... */
  g_hash_table_remove_all(spin->updated_status_list); 

  spin_fetch_json_request
    (spin,"http://www.spin.de/api/friends",
     spin_receive_friends_cb,spin->gc,
     "session",spin->session,
     "photo","1",
     "utf8","1",
     NULL);
}
