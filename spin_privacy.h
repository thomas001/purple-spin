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

#ifndef SPIN_PRIVACY_H_
#define SPIN_PRIVACY_H_

#include "spin.h"

void spin_sync_privacy_lists(SpinData* spin);
void spin_sync_privacy_policy(SpinData* spin);

void spin_set_permit_deny(PurpleConnection* gc);

void spin_add_permit(PurpleConnection* gc,const gchar* name);
void spin_add_deny(PurpleConnection* gc,const gchar* name);
void spin_rem_permit(PurpleConnection* gc,const gchar* name);
void spin_rem_deny(PurpleConnection* gc,const gchar* name);

void spin_ignore_user(SpinData* spin,const gchar* name);
void spin_unignore_user(SpinData* spin,const gchar* name);


#endif
