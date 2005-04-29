/*
 * MU-Conference - Multi-User Conference Service
 * Copyright (c) 2002 David Sutton
 *
 *
 * This program is free software; you can redistribute it and/or drvify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA02111-1307USA
 */

#include "conference.h"

gboolean remove_key(gpointer key, gpointer data, gpointer arg)
{
   log_debug(NAME, "[%s] Auto-removing key %s", FZONE, key);

   free(key);
   free(data);
   return TRUE;
}

void ght_remove_key(gpointer data)
{
   log_debug(NAME, "[%s] Auto-removing key %s", FZONE, data);
   free(data);
}
                                                                                                                  
void ght_remove_cnu(gpointer data)
{
   cnu node = (cnu)data;
   log_debug(NAME, "[%s] Auto-removing cnu %s", FZONE, jid_full(jid_fix(node->realid)));
   pool_free(node->p);
}
                                                                                                                  
void ght_remove_cnr(gpointer data)
{
   cnr node = (cnr)data;
   log_debug(NAME, "[%s] Auto-removing cnr %s", FZONE, jid_full(jid_fix(node->id)));
   pool_free(node->p);
}
                                                                                                                  
void ght_remove_xmlnode(gpointer data)
{
   xmlnode node = (xmlnode)data;
   log_debug(NAME, "[%s] Auto-removing xmlnode (%s)", FZONE, xmlnode2str(node));
   pool_free(node->p);
}

