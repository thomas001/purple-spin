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

#include "spin_userinfo.h"
#include <libxml/HTMLparser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

#include "spin_web.h"

typedef struct _PicInfo
{
  gchar* who;
  PurpleNotifyUserInfo* ui;
  PurpleConnection* gc;
} PicInfo;

static gchar* spin_get_nodeset_content(xmlNodeSetPtr ns)
{
  if(!ns)
    return g_strdup("");
  
  gint i;
  GString* out = g_string_new("");
  for( i = 0; i < ns->nodeNr; ++i)
    {
      xmlNodePtr node = ns->nodeTab[i];
      gchar* ctxt = (gchar*) xmlNodeGetContent(node);
      if(!ctxt)
	continue;
      g_string_append(out,ctxt);
      xmlFree(ctxt);
    }
  return g_string_free(out,FALSE);
}

static void get_head_info(PurpleNotifyUserInfo* ui,xmlXPathContextPtr ctxt)
{
  static xmlXPathCompExprPtr xpath = NULL;
  if(!xpath) /* race condition */
    {
      xpath = xmlXPathCompile((xmlChar*)"string(//div[@class='sbox']/p)");
      g_assert(xpath);
    }

  g_return_if_fail(ui);
  g_return_if_fail(ctxt);

  xmlXPathObjectPtr res = xmlXPathCompiledEval(xpath,ctxt);
  g_return_if_fail(res);

  if(res->type == XPATH_STRING && *res->stringval)
    {
      gchar* escaped = g_markup_escape_text((gchar*) res->stringval,-1);
      purple_notify_user_info_add_pair(ui,escaped,NULL);
      purple_notify_user_info_add_section_break(ui);
      g_free(escaped);
    }

  xmlXPathFreeObject(res);  
}

static gchar* get_img_info(PurpleNotifyUserInfo* ui,xmlXPathContextPtr ctxt)
{
  static xmlXPathCompExprPtr xpath = NULL;
  gchar* val = NULL;
  if(!xpath) /* race condition */
    {
      xpath = xmlXPathCompile((xmlChar*)"string(//img[@class='thumb']/@src)");
      g_assert(xpath);
    }

  g_return_val_if_fail(ctxt,NULL);
  g_return_val_if_fail(ui,NULL);

  xmlXPathObjectPtr res = xmlXPathCompiledEval(xpath,ctxt);
  g_return_val_if_fail(res,NULL);

  if(res->type == XPATH_STRING && *res->stringval)
    {
      purple_notify_user_info_add_pair(ui,_("Image"),_("loading..."));
      purple_notify_user_info_add_section_break(ui);
      GRegex* regex = g_regex_new("/mini/",0,0,NULL);
      val = g_regex_replace_literal(regex,(gchar*)res->stringval,-1,0,"/full/",
				    0,NULL);
      /* val = g_strdup((gchar*) res->stringval); */
      g_regex_unref(regex);
    }

  xmlXPathFreeObject(res);
  return val;
}

static void get_profile_info(PurpleNotifyUserInfo* ui,xmlXPathContextPtr ctxt)
{
  static xmlXPathCompExprPtr label_xpath = NULL;
  static xmlXPathCompExprPtr siblings_xpath = NULL;
  if(!label_xpath) /* race condition */
    {
      label_xpath = xmlXPathCompile((xmlChar*)"//*[@class='label']");
      g_assert(label_xpath);
    }
  if(!siblings_xpath)
    {
      siblings_xpath = xmlXPathCompile((xmlChar*)"following-sibling::text()"
				       "|following-sibling::*");
      g_assert(siblings_xpath);
    }

  g_return_if_fail(ui);
  g_return_if_fail(ctxt);

  xmlXPathObjectPtr res = xmlXPathCompiledEval(label_xpath,ctxt);
  g_return_if_fail(res);

  if(res->type == XPATH_NODESET)
    {
      xmlNodePtr old_node = ctxt->node;
      gint i;
      for(i = 0; i < res->nodesetval->nodeNr; ++i)
	{
	  xmlNodePtr node = res->nodesetval->nodeTab[i];
	  ctxt->node = node;
	  xmlXPathObjectPtr local_res = xmlXPathCompiledEval(siblings_xpath,
							     ctxt);
	  if(!local_res)
	    continue;
	  xmlChar* label = xmlNodeGetContent(node);
	  if(local_res->type == XPATH_NODESET)
	    {
	      gchar* content = spin_get_nodeset_content(local_res->nodesetval);
	      gchar* escaped_label = g_markup_escape_text((gchar*) label,-1);
	      gchar* escaped_content = g_markup_escape_text(content,-1);
	      purple_notify_user_info_add_pair(ui,escaped_label,
					       escaped_content);
	      g_free(escaped_content);
	      g_free(escaped_label);
	      g_free(content);
	    }
	  xmlXPathFreeObject(local_res);
	  xmlFree(label);
	}
      if(i > 0)
	purple_notify_user_info_add_section_break(ui);
      ctxt->node = old_node;
    }

  xmlXPathFreeObject(res);  
}

static void spin_pic_cb(PurpleUtilFetchUrlData* url_data,gpointer userp,
			const gchar* data,gsize len,
			const gchar* error_message)
{

  PicInfo* pic_info = (PicInfo*) userp;
  PurpleConnection* gc = pic_info->gc;
  gint pic_id = 0;

  if(!PURPLE_CONNECTION_IS_VALID(gc))
    goto exit;

  if(data)
    {
      gpointer copied = g_memdup(data,len);
      pic_id = purple_imgstore_add_with_id(copied,len,NULL);
    }

  GList* entries = purple_notify_user_info_get_entries(pic_info->ui);
  for(;entries;entries = g_list_next(entries))
    {
      PurpleNotifyUserInfoEntry* entry = entries->data;
      const gchar* label = purple_notify_user_info_entry_get_label(entry);
      if(g_strcmp0(label,_("Image")) != 0)
	continue;
      if(pic_id)
	{
	  gchar* text = g_strdup_printf("<img id='%i'>",pic_id);
	  purple_notify_user_info_entry_set_value(entry,text);
	  g_free(text);
	}
      else if(error_message)
	purple_notify_user_info_entry_set_value(entry,error_message);
      else
	purple_notify_user_info_entry_set_value(entry,_("HTTP error"));
      break;
    }

  purple_notify_userinfo(gc,pic_info->who,pic_info->ui,NULL,NULL);

 exit:
  if(pic_id)
    purple_imgstore_unref_by_id(pic_id);
  g_free(pic_info->who);
  purple_notify_user_info_destroy(pic_info->ui);
  g_free(pic_info);
}

typedef struct InfoData_
{
  PurpleConnection* gc;
  gchar* user;
} InfoData;

static void spin_info_cb(PurpleUtilFetchUrlData* url_text,
			 gpointer userp,
			 const gchar* data,
			 gsize len,
			 const gchar* error_message)
{
  htmlDocPtr doc = NULL;
  xmlXPathContextPtr ctxt = NULL;
  InfoData* info = (InfoData*) userp;

  PurpleNotifyUserInfo* ui = purple_notify_user_info_new();
  gchar* who = info->user, *image_url = NULL;
  PurpleConnection* gc = info->gc;

  if(!PURPLE_CONNECTION_IS_VALID(gc) || !gc->proto_data)
    goto exit;

  if(!data)
    {
      purple_notify_user_info_add_pair(ui,_("Error"),error_message);
      purple_notify_userinfo(gc,who,ui,NULL,NULL);
      goto exit;
    }

  SpinData* spin = (SpinData*) gc->proto_data;
  doc = htmlReadMemory((char*) data,len,"",NULL,0);
  if(doc && (ctxt = xmlXPathNewContext(doc)))
    {
      get_head_info(ui,ctxt);
      image_url = get_img_info(ui,ctxt);
      get_profile_info(ui,ctxt);
    }
  else if(!doc)
    {
      purple_notify_user_info_add_pair(ui,_("Error"),_("HTML parse error"));
      purple_notify_user_info_add_section_break(ui);
    }

  gchar* escaped_who = g_uri_escape_string(who,NULL,FALSE);
  gchar* url = spin_session_url(spin,"/hp/%s/",escaped_who);
  purple_notify_user_info_add_pair(ui,_("More Info"),url);
  g_free(escaped_who);
  g_free(url);

  purple_notify_userinfo(gc,who,ui,NULL,NULL);

  if(image_url)
    {
      PicInfo* pic_info = g_new(PicInfo,1);
      pic_info->ui = ui;
      pic_info->who = who;
      pic_info->gc = gc;
      spin_fetch_url_request(spin,image_url,spin_pic_cb,pic_info);
      goto exit_image;
    }

 exit:
  g_free(who);
  purple_notify_user_info_destroy(ui);
 exit_image:
  g_free(info);
  if(ctxt)
    xmlXPathFreeContext(ctxt);
  if(doc)
    xmlFreeDoc(doc);
  g_free(image_url);
}


void spin_get_info(PurpleConnection* gc,const gchar* who)
{
  SpinData* spin = (SpinData*) gc->proto_data;
  if(!spin->session)
    return;

  gchar* escaped_who = g_uri_escape_string(who,NULL,FALSE);
  gchar* url = g_strdup_printf("http://www.spin.de/hp/%s/",escaped_who);
  g_free(escaped_who);

  InfoData* info = g_new(InfoData,1);
  info->gc = gc;
  info->user = g_strdup(who);
  spin_fetch_url_request(spin,url,spin_info_cb,info);
  g_free(url);
}

