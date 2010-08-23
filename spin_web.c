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

#include "spin_web.h"
#include <stdarg.h>
#include <string.h>

PurpleUtilFetchUrlData* purple_util_fetch_url_request_len_with_account
(PurpleAccount *   	 account,
 const gchar *  	url,
 gboolean  	full,
 const gchar *  	user_agent,
 gboolean  	http11,
 const gchar *  	request,
 gboolean  	include_headers,
 gssize  	max_len,
 PurpleUtilFetchUrlCallback  	callback,
 gpointer  	data	 
 );

typedef struct WebJsonData_
{
  SpinFetchJsonCallback callback;
  gpointer userdata;
} WebJsonData;

static void spin_web_json_cb(PurpleUtilFetchUrlData *url_data,
			     gpointer user_data,
			     const gchar *url_text, gsize len,
			     const gchar *error_message)
{
  WebJsonData* data = (WebJsonData*) user_data;
  static GRegex *string_literal_re;
  gchar *fixed = NULL;
  JsonParser *parser = NULL;
  GError *error = NULL;
  
  if(!url_text)
    {
      data->callback(url_data,data->userdata,NULL,error_message);
      goto exit;
    }

  if(!string_literal_re) /* TODO: race condition */
    {
      GError* error = NULL;
      string_literal_re = g_regex_new
	("(['\"])((?:(?!\\1)[^\\x00-\\x1f\\\\]||\\\\[\\\\/bfnrt]|\\\\\\1"
	 "|\\\\u[0-9a-fA-F]{4}|\\\\[\\x20-\\xff])*)\\1",0,0,&error);
      g_assert(error == NULL);
    }
  
  parser = json_parser_new();
  fixed = g_regex_replace(string_literal_re,
			  url_text,len,0,
			  "\"\\2\"",0,&error);
  g_assert(error == NULL);
  g_strstrip(fixed);
  json_parser_load_from_data(parser,fixed,-1,&error);
  if(error)
    {
      data->callback(url_data,data->userdata,NULL,
			  error->message);
      g_error_free(error);
    }
  else
    {
      data->callback(url_data,data->userdata,json_parser_get_root(parser),
		     NULL);
    }

 exit:
  if(parser)
    g_object_unref(parser);
  g_free(fixed);
  g_free(data);
}

static gchar* urlform_encode(const gchar* p)
{
  gchar* out = g_uri_escape_string(p," ",FALSE);
  gchar* i = out;
  while((i = strchr(i,' ')))
    *i++ = '+';
  return out;
}

static GString* spin_vcollect_params(va_list params)
{
  GString *out = g_string_new("");
  const gchar *key,*value;

  while((key = va_arg(params,const gchar*)))
    {
      gchar *escaped_key,*escaped_value;
      value = va_arg(params,const gchar*);
      escaped_key = urlform_encode(key);
      escaped_value = urlform_encode(value);
      if(out->len > 0)
	g_string_append_c(out,'&');
      g_string_append(out,escaped_key);
      g_string_append_c(out,'=');
      g_string_append(out,escaped_value);

      g_free(escaped_key);
      g_free(escaped_value);
    }

  return out;
}


PurpleUtilFetchUrlData* spin_fetch_json_request
(SpinData* spin,const gchar* url,
 SpinFetchJsonCallback callback,gpointer userdata,
 ...)
{
  va_list ap;
  
  WebJsonData* data = g_new(WebJsonData,1);
  data->callback = callback;
  data->userdata = userdata;

  va_start(ap,userdata);

  PurpleUtilFetchUrlData* url_data = 
    spin_vfetch_post_request(spin,url,spin_web_json_cb,data,ap);

  va_end(ap);

  return url_data;
}

PurpleUtilFetchUrlData* spin_fetch_post_request
(SpinData* spin,const gchar* url,
 PurpleUtilFetchUrlCallback callback,gpointer userdata,
 ...)
{
  va_list ap;
  va_start(ap,userdata);
  PurpleUtilFetchUrlData* url_data =
    spin_vfetch_post_request(spin,url,callback,userdata,ap);
  va_end(ap);
  return url_data;
}

PurpleUtilFetchUrlData* spin_vfetch_post_request
(SpinData* spin,const gchar* url,
 PurpleUtilFetchUrlCallback callback,gpointer userdata,
 va_list args)
{
  gchar *host,*path,*user,*passwd,*req,*cookie_header=NULL;
  gint port;
  GString* post_data;
  PurpleAccount* account;

  g_return_val_if_fail(spin,NULL);
  g_return_val_if_fail(url,NULL);
  g_return_val_if_fail(callback,NULL);
  
  if(!purple_url_parse(url,&host,&port,&path,&user,&passwd))
    return NULL;

  account = purple_connection_get_account(spin->gc);

  post_data = spin_vcollect_params(args);

  if(spin->session)
    cookie_header = g_strdup_printf("Cookie:session=%s;session2=%s\r\n",
				    spin->session,spin->session);

  req = g_strdup_printf("POST /%s HTTP/1.0\r\n"
			"Host:%s\r\n"
			"Content-Type:application/x-www-form-urlencoded\r\n"
			"Content-Length:%" G_GSIZE_FORMAT "\r\n"
			"Connection:close\r\n"
			"%s"
			"\r\n%s",
			path ? path : "",host,post_data->len,
			cookie_header ? cookie_header : "",
			post_data->str);


  PurpleUtilFetchUrlData* url_data =
    purple_util_fetch_url_request_len
    (url,
     TRUE, /* full url */
     NULL, /* user agent */
     FALSE, /* http 1.1 */
     req, /* request */
     FALSE, /* include headers */
     -1, /* max len */
     callback, /* callback */
     userdata);

  g_free(req); 
  g_free(cookie_header);
  g_string_free(post_data,TRUE);
  g_free(host);
  g_free(path);
  g_free(user);
  g_free(passwd);

  return url_data;
}

PurpleUtilFetchUrlData* spin_fetch_url_request
(SpinData* spin,const gchar* url,
 PurpleUtilFetchUrlCallback callback,gpointer userdata)
{
  gchar *host,*path,*user,*passwd,*req,*cookie_header=NULL;
  gint port;
  PurpleAccount* account;

  g_return_val_if_fail(spin,NULL);
  g_return_val_if_fail(url,NULL);
  g_return_val_if_fail(callback,NULL);
  
  if(!purple_url_parse(url,&host,&port,&path,&user,&passwd))
    return NULL;

  account = purple_connection_get_account(spin->gc);

  if(spin->session)
    cookie_header = g_strdup_printf("Cookie:session=%s;session2=%s\r\n",
				    spin->session,spin->session);

  req = g_strdup_printf("GET /%s HTTP/1.0\r\n"
			"Host:%s\r\n"
			"Connection:close\r\n"
			"%s"
			"\r\n",
			path ? path : "",host,
			cookie_header ? cookie_header : "");

  PurpleUtilFetchUrlData* url_data =
    purple_util_fetch_url_request_len
    (url,
     TRUE, /* full url */
     NULL, /* user agent */
     FALSE, /* http 1.1 */
     req, /* request */
     FALSE, /* include headers */
     -1, /* max len */
     callback, /* callback */
     userdata);

  g_free(req); 
  g_free(cookie_header);
  g_free(host);
  g_free(path);
  g_free(user);
  g_free(passwd);

  return url_data;
}

