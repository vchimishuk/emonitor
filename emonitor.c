/* emonitor plugin for fbpanel application.
 * Copyright (C) 2009 by Viacheslav Chumushuk (viacheslav88@gmail.com)
 * Website: http://code.google.com/p/emonitor/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "plugin.h"
#include "panel.h"
#include "misc.h"

//#define DEBUG
#include "dbg.h"

/* Maximum length of status string */
#define STATUS_MAX_LENGTH 1024


/*
 * Emonitor structure
 */
struct emonitor_t {
  /* Main window widget */
  GtkWidget *main_win;
  /* Label widget */
  GtkWidget *label_win;
  /* Width of widget, or -1 for unuse */
  gint width;
  /* External program name to get status string */
  gchar *action;
  /* External program to execute by user click */
  gchar *onclick_action;
  /* Timer tick time in ms*/
  gint tick;
  /* Update monitor timer */
  int timer;
  /* Current status */
  gchar status[STATUS_MAX_LENGTH];
};


/*
 * Delete all white spaces from the end of the string.
 * Returns the parameter pointer.
 */
char *rtrim(char *str)
{
  if(str == NULL)
    return NULL;

  char *end = str + strlen(str) - 1;

  while(end > str && isspace((int)*end))
    end--;

  *(end + 1) = '\0';

  return str;
}


/*
 * Run external program and fill status with its output.
 */
void emonitor_get_status(struct emonitor_t *emonitor)
{
  FILE *fp;

  fp = popen(emonitor->action, "r");
  if(!fp) {
    ERR("emonitor: failed to run external program.\n");
    return;
  }

  fgets(emonitor->status, STATUS_MAX_LENGTH - 1, fp);
  rtrim(emonitor->status);

  pclose(fp);
}


/*
 * This function calls by GTK engine when user click on the monitor.
 */
static gboolean emonitor_onclick(GtkWidget *widget, gpointer dummy, struct emonitor_t *emonitor)
{
  ENTER;
  
  if(emonitor->onclick_action && strlen(emonitor->onclick_action))
    system(emonitor->onclick_action);
    
  RET(TRUE);
}


/*
 * Tick timer event procedure.
 */
static gint emonitor_update(gpointer data)
{
  struct emonitor_t *emonitor = (struct emonitor_t *)data;
  
  ENTER;

  emonitor_get_status(emonitor);
  gtk_label_set_markup(GTK_LABEL(emonitor->label_win), emonitor->status);

  RET(TRUE);
}


/*
 * Calls when module is loaded.
 */
static int emonitor_constructor(plugin *p)
{
  struct emonitor_t *emonitor;
  line config_line;

  ENTER;
  emonitor = g_new0(struct emonitor_t, 1);
  g_return_val_if_fail(emonitor != NULL, FALSE);


  /* Read configurations. */
  emonitor->action = NULL;
  emonitor->onclick_action = NULL;
  emonitor->tick = 1000;

  config_line.len = 256;
  while(get_line(p->fp, &config_line) != LINE_BLOCK_END) {
    if(config_line.type == LINE_NONE) {
      ERR("emonitor: illegal token %s\n", config_line.str);
      goto error;
    }

    if(config_line.type == LINE_VAR) {
      if(!g_ascii_strcasecmp(config_line.t[0], "UpdateInterval")) {
	emonitor->tick = atoi(config_line.t[1]);
      } else if(!g_ascii_strcasecmp(config_line.t[0], "ExternalCommand")) {
	emonitor->action = g_strdup(config_line.t[1]);
      } else if(!g_ascii_strcasecmp(config_line.t[0], "OnClickCommand")) {
	emonitor->onclick_action = g_strdup(config_line.t[1]);
      } else if(!g_ascii_strcasecmp(config_line.t[0], "Width")) {
	emonitor->width = atoi(config_line.t[1]);
      } else {
	ERR("emonitor: unknown var %s\n", config_line.t[0]);
	goto error;
      }
    } else {
      ERR("emonitor: unknown var %s\n", config_line.str);
      goto error;
    }
  }

  emonitor_get_status(emonitor);
  p->priv = emonitor;

  /* Create widgets */
  emonitor->main_win = p->panel->my_box_new(TRUE, 1);
  emonitor->main_win = gtk_event_box_new();
  g_signal_connect(G_OBJECT(emonitor->main_win), "button_press_event",
		   G_CALLBACK(emonitor_onclick), (gpointer)emonitor);
  emonitor->label_win = gtk_label_new(emonitor->status);
  gtk_misc_set_alignment(GTK_MISC(emonitor->label_win), 0.5, 0.5);
  gtk_misc_set_padding(GTK_MISC(emonitor->label_win), 4, 0);
  gtk_container_add(GTK_CONTAINER(emonitor->main_win), emonitor->label_win);  

  gtk_widget_set_size_request(emonitor->label_win, emonitor->width, -1);

  gtk_widget_show_all(emonitor->main_win);
  gtk_container_add(GTK_CONTAINER(p->pwid), emonitor->main_win);
  
  emonitor->timer = g_timeout_add(emonitor->tick, (GSourceFunc)emonitor_update, (gpointer)emonitor);

  RET(TRUE);

 error:
  g_free(emonitor->action);
  g_free(emonitor);
  
  RET(FALSE);
}


/*
 * Calls before removing module.
 */
static void emonitor_destructor(plugin *p)
{
  struct emonitor_t *emonitor = (struct emonitor_t *)p->priv;

  ENTER;
  gtk_widget_destroy(emonitor->main_win);
  if(emonitor->timer)
    g_source_remove(emonitor->timer);
  g_free(emonitor->action);
  g_free(emonitor);

  RET();
}


/*
 * Plugin object.
 */ 
plugin_class emonitor_plugin_class = {
 fname: NULL,
 count: 0,

 type: "emonitor",
 name: "External monitor",
 version: "0.1",
 description: "Show output of external program. Author: Viacheslav Chumushuk <viacheslav88@gmail.com>",

 constructor: emonitor_constructor,
 destructor: emonitor_destructor
};
