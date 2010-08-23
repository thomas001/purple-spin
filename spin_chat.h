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

#ifndef SPIN_CHAT_H_
#define SPIN_CHAT_H_

#include "spin.h"

GList* spin_chat_info(PurpleConnection* conn);
GHashTable* spin_chat_info_defaults(PurpleConnection* gc,const gchar* name);
PurpleRoomlist* spin_roomlist_get_list(PurpleConnection* gc);
void spin_roomlist_cancel(PurpleRoomlist* list);
void spin_chat_join(PurpleConnection* gc,GHashTable* data);
gchar* spin_get_chat_name(GHashTable* data);
void spin_chat_leave(PurpleConnection* gc,gint id);
int spin_chat_send(PurpleConnection* gc,int id,const gchar* msg,
		    PurpleMessageFlags flags);
gchar* spin_encode_room(const gchar* room);

void spin_chat_set_room_away(SpinData* spin,const gchar* room,gboolean away);
void spin_chat_set_room_status(SpinData* spin,const gchar* room,PurpleStatus* status);

#endif
