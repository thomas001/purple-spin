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

#include "spin_mail.h"
#include "spin_web.h"
#include "spin_login.h"
#include "debug.h"

static void spin_got_mail(PurpleUtilFetchUrlData* url_text,gpointer userp,
			  JsonNode* node,const gchar* error_message)
{
  PurpleConnection *gc = (PurpleConnection*) userp;
  
  if(!PURPLE_CONNECTION_IS_VALID(gc) || !gc->proto_data)
    return;
  
  SpinData* spin = (SpinData*) gc->proto_data;

  if(!node)
    {
      purple_debug_info("spin","could not fetch mail: %s\n",error_message);
      goto exit;
    }

  if(JSON_NODE_TYPE(node) != JSON_NODE_ARRAY)
    goto exit;
  JsonArray* array = json_node_get_array(node);

  PurpleAccount* account = purple_connection_get_account(gc);
  const gchar* user = spin->username;

  const gchar* last_check =
    purple_account_get_string(account,"last-mail-check","");
  const gchar* new_last_check = last_check;

  gint i;
  for(i = 0; i < json_array_get_length(array); ++i)
    {
      node = json_array_get_element(array,i);
      if(JSON_NODE_TYPE(node) != JSON_NODE_ARRAY)
	continue;
      JsonArray* entry = json_node_get_array(node);
      if(json_array_get_length(entry) < 8)
	continue;

      const gchar* id =json_node_get_string(json_array_get_element(entry,0));
      const gchar* state =json_node_get_string(json_array_get_element(entry,2));
      const gchar* subj =json_node_get_string(json_array_get_element(entry,3));
      const gchar* frm =json_node_get_string(json_array_get_element(entry,5));
      const gchar* arri =json_node_get_string(json_array_get_element(entry,7));

      if(!id || !state || !subj || !frm || !arri)
	continue;

      gchar* endp;
      gint64 int_id = g_ascii_strtoll(id,&endp,10);
      if(*endp)
	{
	  purple_debug_info("spin","invalid integer in mail reply: %s\n",id);
	  continue;
	}

      if(g_strcmp0(state,"new") != 0)
	continue;

      if(g_strcmp0(arri,last_check) <= 0)
	continue;

      if(g_strcmp0(arri,new_last_check) > 0)
	new_last_check = arri;

      gchar* url = spin_session_url(spin,"/mail/display?hid=%lx",int_id);

      purple_notify_email(gc,subj,frm,user,url,NULL,NULL);

      g_free(url);
    }

  purple_account_set_string(account,"last-mail-check",new_last_check);

 exit:;
  spin_connect_add_state(spin,SPIN_STATE_GOT_INITIAL_MAIL_LIST);
}

void spin_check_mail(SpinData* spin)
{
  g_return_if_fail(spin);
  g_return_if_fail(spin->session);

  spin_fetch_json_request(spin,"http://www.spin.de/api/readmail",
			  spin_got_mail,spin->gc,
			  NULL);
}
