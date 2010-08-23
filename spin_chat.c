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

#include "spin_chat.h"

#include "prpl.h"
#include "debug.h"
#include <string.h>

gchar* spin_encode_room(const gchar* room)
{
  gsize bytes_in,bytes_out,len = strlen(room);
  GError* error = NULL;
  gchar* out = g_convert(room,len,"ISO-8859-15","UTF-8",&bytes_in,&bytes_out,
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

GList* spin_chat_info(PurpleConnection* gc)
{
  GList* r = NULL;
  struct proto_chat_entry* pce;

  pce = g_new0(struct proto_chat_entry,1);
  pce->label = _("Room name:");
  pce->identifier = "room";
  pce->required = TRUE;
  r = g_list_append(r,pce);

  return r;
}

GHashTable* spin_chat_info_defaults(PurpleConnection* gc,const gchar* name)
{
  GHashTable* defaults =
    g_hash_table_new_full(g_str_hash,g_str_equal,NULL,g_free);

  if(name != NULL)
    g_hash_table_insert(defaults,"room",g_strdup(name));

  return defaults;
}

PurpleRoomlist* spin_roomlist_get_list(PurpleConnection* gc)
{
  SpinData* spin = (SpinData*) gc->proto_data;
  if(spin->roomlist)
    purple_roomlist_unref(spin->roomlist);

  spin->roomlist = purple_roomlist_new(purple_connection_get_account(gc));

  PurpleRoomlistField* f;
  GList* fields = NULL;
  f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING,"","room",TRUE); 
  fields = g_list_append(fields,f); 
  /* f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_INT,"Users","users",TRUE); */
  /* fields = g_list_append(fields,f); */

  purple_roomlist_set_fields(spin->roomlist,fields);

  spin_write_command(spin,'l',NULL);
  purple_roomlist_set_in_progress(spin->roomlist,TRUE);

  return spin->roomlist;
}

void spin_roomlist_cancel(PurpleRoomlist* list)
{
  PurpleConnection* gc = purple_account_get_connection(list->account);
  if(!gc)
    return;

  SpinData* spin = (SpinData*) gc->proto_data;

  purple_roomlist_set_in_progress(list,FALSE);

  if(spin->roomlist == list)
    {
      spin->roomlist = NULL;
      purple_roomlist_unref(list);
    }
}

void spin_chat_join(PurpleConnection* gc,GHashTable* data)
{
  SpinData* spin = (SpinData*) gc->proto_data;
  gchar* room_name = g_hash_table_lookup(data,"room");
  gchar* encoded_room_name = NULL;
  PurpleAccount* account = purple_connection_get_account(gc);

  if(strchr(room_name,'#'))
    {
      purple_notify_error(gc,_("Invalid room name"),
			  _("Room names must not contain #"),
			  NULL);
      purple_serv_got_join_chat_failed(gc,data);
      return;
    }

  if(!(encoded_room_name = spin_encode_room(room_name)))
    {
      purple_notify_error(gc,_("Invalid room name"),
			  _("Room name contains invalid characters"),
			  NULL);
      purple_serv_got_join_chat_failed(gc,data);
      return;
    }

  g_hash_table_insert(spin->pending_joins,
		      g_strdup(purple_normalize(account,room_name)),
		      GINT_TO_POINTER(1));
  spin_write_command(spin,'c',encoded_room_name,NULL);
  g_free(encoded_room_name);
}

void spin_chat_leave(PurpleConnection* gc,gint id)
{
  SpinData* spin = (SpinData*) gc->proto_data;
  if(!spin)
    return;
  
  PurpleConversation* conv = purple_find_chat(gc,id);
  if(!conv)
    return;

  PurpleAccount* account = purple_connection_get_account(gc);
  const gchar* name = purple_conversation_get_name(conv);
  gchar* name_2;
  if(!(name_2 = spin_encode_room(name)))
    return;
  g_hash_table_remove(spin->pending_joins,purple_normalize(account,name));
  spin_write_command(spin,'d',name_2,NULL);
  g_free(name_2);
}

gchar* spin_get_chat_name(GHashTable* data)
{
  return g_strdup(g_hash_table_lookup(data,"room"));
}

int spin_chat_send(PurpleConnection* gc,int id,const gchar* msg,
		    PurpleMessageFlags flags)
{
  SpinData* spin = (SpinData*) gc->proto_data;
  PurpleConversation* conv = purple_find_chat(gc,id);
  if(!conv)
    return -1;

  const gchar* room = purple_conversation_get_name(conv);
  gchar* room_2;
  if(!(room_2 = spin_encode_room(room)))
    return -1;
  gchar* msg_2 = spin_convert_out_text(msg);
  gchar* ty = "a";
  if(purple_message_meify(msg_2,-1))
    ty = "c";

  spin_write_command(spin,'g',room_2,ty,msg_2,NULL);

  g_free(room_2);
  g_free(msg_2);

  return 1;
}

void spin_chat_set_room_away(SpinData* spin,const gchar* name,gboolean away)
{
  gchar* encoded_name = spin_encode_user(name);
  g_return_if_fail(encoded_name);
  spin_write_command(spin,'g',encoded_name,"0","away",away ? "1" : "0",NULL);
  g_free(encoded_name);
}


void spin_chat_set_room_status(SpinData* spin,const gchar* room,PurpleStatus* status)
{
  PurpleStatusPrimitive primitive =
    purple_status_type_get_primitive(purple_status_get_type(status));
  gboolean away = (primitive == PURPLE_STATUS_AWAY)
    || (primitive == PURPLE_STATUS_EXTENDED_AWAY);
  spin_chat_set_room_away(spin,room,away);
}
