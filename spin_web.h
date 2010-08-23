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

#ifndef SPIN_WEB_H_
#define SPIN_WEB_H_

#include "spin.h"
#include "util.h"
#include <json-glib/json-glib.h>

typedef void (*SpinFetchJsonCallback)(PurpleUtilFetchUrlData* fetch_data,
				      gpointer userdata,
				      JsonNode* node,
				      const gchar* error_message);


PurpleUtilFetchUrlData* spin_fetch_json_request
(SpinData* spin,const gchar* url,
 SpinFetchJsonCallback callback,gpointer userdata,
 ...) G_GNUC_NULL_TERMINATED;

PurpleUtilFetchUrlData* spin_fetch_url_request
(SpinData* spin,const gchar* url,
 PurpleUtilFetchUrlCallback callback,gpointer userdata);

PurpleUtilFetchUrlData* spin_fetch_post_request
(SpinData* spin,const gchar* url,
 PurpleUtilFetchUrlCallback callback,gpointer userdata,
 ...) G_GNUC_NULL_TERMINATED;

PurpleUtilFetchUrlData* spin_vfetch_post_request
(SpinData* spin,const gchar* url,
 PurpleUtilFetchUrlCallback callback,gpointer userdata,
 va_list ap);

#endif
