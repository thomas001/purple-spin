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

#include "spin_prefs.h"
#include "spin_web.h"
#include "spin_login.h"

#include "privacy.h"

static void spin_prefs_cb(PurpleUtilFetchUrlData* url_data,
			  gpointer userp,JsonNode* node,
			  const gchar* error_message)
{
  PurpleConnection* gc = (PurpleConnection*) userp;
  JsonNode *prefsok/* ,*diablock */;
  JsonObject *object;
  /* gint diablock_value; */
  PurpleAccount* account;
  SpinData* spin;

  if(!PURPLE_CONNECTION_IS_VALID(gc))
    return;

  spin = (SpinData*) gc->proto_data;
  account = purple_connection_get_account(gc);

  if(!node)
    {
      gchar* err_text = g_strdup_printf(_("Could not receive prefs: %s"),
					error_message);
      purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				     err_text);
      g_free(err_text);
      return;
    }

  if(JSON_NODE_TYPE(node) != JSON_NODE_OBJECT)
    {
      purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				     _("Invalid prefs format received"));
      return;
    }

  object = json_node_get_object(node);
  
  prefsok = json_object_get_member(object,"prefsok");

  if(!prefsok || JSON_NODE_TYPE(prefsok) != JSON_NODE_VALUE
     || !json_node_get_int(prefsok))
    {
      purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
				     _("Prefs not OK"));
      return;
    }

  /* diablock = json_object_get_member(object,"diablock"); */
  /* if(!diablock || JSON_NODE_TYPE(diablock) != JSON_NODE_VALUE) */
  /*   { */
  /*     purple_connection_error_reason(gc,PURPLE_CONNECTION_ERROR_NETWORK_ERROR, */
  /* 				     _("Invalid prefs entry received")); */
  /*     return; */
  /*   } */
  
  /* if(json_node_get_value_type(diablock) == G_TYPE_STRING) */
  /*   diablock_value = g_ascii_strtoll(json_node_get_string(diablock),NULL,10); */
  /* else */
  /*   diablock_value = json_node_get_int(diablock); */

  /* switch(diablock_value) */
  /*   { */
  /*   case 0: */
  /*     account->perm_deny = PURPLE_PRIVACY_DENY_USERS; */
  /*     break; */
  /*   case 1: */
  /*     account->perm_deny = PURPLE_PRIVACY_DENY_ALL; */
  /*     break; */
  /*   case 2: */
  /*     account->perm_deny = PURPLE_PRIVACY_ALLOW_BUDDYLIST; */
  /*     break; */
  /*   } */

  spin_connect_add_state(spin,SPIN_STATE_GOT_INITIAL_PREFS);
}

void spin_load_prefs(SpinData* spin)
{
  g_return_if_fail(spin);
  g_return_if_fail(spin->session);

  spin_fetch_json_request(spin,"http://www.spin.de/api/prefs",
			  spin_prefs_cb,spin->gc,
			  "session",spin->session,
			  "utf8","1",
			  NULL);
}
