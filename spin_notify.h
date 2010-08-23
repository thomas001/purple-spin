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

#ifndef SPIN_NOTIFY_H_
#define SPIN_NOTIFY_H_

#include "spin.h"

void spin_notify_nick_changed(SpinData* spin,const gchar* old_nick,
			      const gchar* new_nick);
void spin_notify_nick_removed(SpinData* spin,const gchar* nick);
void spin_notify_game_invite(SpinData* spin,const gchar* nick,
			     const gchar* title,const gchar* t,
			     const gchar* id,const gchar* passwd);
void spin_notify_guestbook_entry(SpinData* spin,gchar* who);
void spin_notify_gift(SpinData* spin,gchar* who);
#endif
