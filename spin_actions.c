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

#include "spin_actions.h"
#include "spin.h"

static void open_page(PurplePluginAction* action)
{
  PurpleConnection* gc = (PurpleConnection*) action->context;
  if(!gc || !gc->proto_data)
    return;

  SpinData* spin = (SpinData*) gc->proto_data;
  if(!spin->session)
    return;

  gchar* url = spin_session_url(spin,"%s", (gchar*) action->user_data);
  purple_notify_uri(gc,url);
  g_free(url);
}

static void open_profile(PurplePluginAction* action)
{
  PurpleConnection* gc = (PurpleConnection*) action->context;
  if(!gc || !gc->proto_data)
    return;

  SpinData* spin = (SpinData*) gc->proto_data;
  if(!spin->session)
    return;

  const gchar* username = spin->username;
  gchar* url = spin_session_url(spin,"/hp/%s/",username);
  purple_notify_uri(gc,url);

  g_free(url);
}

static void add_page_action(GList** actions,const gchar* label,
			    const gchar* target)
{
  PurplePluginAction* action = purple_plugin_action_new(label,open_page);
  action->user_data = (gchar*) target;
  *actions = g_list_append(*actions,action);
}

GList* spin_actions(PurplePlugin* plugin, gpointer context)
{
  GList* actions = NULL;
  
  add_page_action(&actions,"Home","/home");
  add_page_action(&actions,"Themen","/themen");
  add_page_action(&actions,"Leute","/leute");
  add_page_action(&actions,"Chat","/chat");
  add_page_action(&actions,"Spiele","/spiele");
  add_page_action(&actions,"Events","/events");

  PurplePluginAction* profile_action
    = purple_plugin_action_new("Profil",open_profile);
  actions = g_list_append(actions,profile_action);

  add_page_action(&actions,"Post","/mail");
  add_page_action(&actions,"Freunde","/relations");

  return actions;
}
