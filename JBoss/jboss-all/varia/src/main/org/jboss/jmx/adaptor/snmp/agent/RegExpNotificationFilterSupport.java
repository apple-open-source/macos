/*
 * Copyright (c) 2003,  Intracom S.A. - www.intracom.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This package and its source code is available at www.jboss.org
**/
package org.jboss.jmx.adaptor.snmp.agent;

import java.util.Map;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Collection;
import java.util.Collections;

import javax.management.NotificationFilter;
import javax.management.Notification;

import gnu.regexp.RE;
import gnu.regexp.REException;

/**
 * <tt>RegExpNotificationFilterSupport</tt> is responsible for checking the
 * matching of notifications against constraints defined by the 
 * notification listener. The constraints are defined as regular expressions 
 * and the filtering is performed on the notification type attribute. The
 * notification type is sequentially checked against the contained regular 
 * expressions until a match is found or all the expressions have been 
 * checked.
 *
 * @version $Revision: 1.1.2.2 $
 *
 * @author  <a href="mailto:spol@intracom.gr">Spyros Pollatos</a>
 * @author  <a href="mailto:andd@intracom.gr">Dimitris Andreadis</a>
**/
public class RegExpNotificationFilterSupport
   implements NotificationFilter
{
   /** Holds the provided regular expression literal as the key and the 
    * corresponding compiled regular expression as the value. Access provided 
    * through synchronized wrapper.
   **/
   protected Map regExps = new HashMap();
   
   /**
    * CTOR
   **/
   public RegExpNotificationFilterSupport()
   {
      // empty
   }
   
   /**
    * Disables all notification types.
   **/ 
   public void disableAllTypes()
   {
      synchronized(this.regExps) {
         this.regExps.clear();    
      }
   }     
    
   /**
    * Removes the given regular expression from the prefix list.
   **/
   public void disableType(String regExp)
   {
      synchronized(this.regExps) {
         this.regExps.remove(regExp);
      }    
   }
    
   /**
    * Enables all the notifications the type of which matches the defined
    * regular expressions to be sent to the listener.
    *
    * @param regExp the regular expression to be added 
    * @throws REException thrown if the expression's syntax is invalid
   **/
   public void enableType(String regExp)
      throws REException
   {
      // Create a pattern to match the provided regular expression and have
      // it added in the pool of maintained expressions. The expression 
      // literal is used as the key while the compiled pattern is the value
      synchronized(this.regExps) {
         this.regExps.put(regExp, new RE(regExp));
      }
   }
    
   /**
    * Gets all the enabled notification types for this filter. The returned
    * collection can not be modified
   **/ 
   public Collection getEnabledTypes()
   {
      synchronized(this.regExps) {
         return Collections.unmodifiableSet(this.regExps.keySet());
      }
   }
          
   /**
    * Invoked before sending the specified notification to the listener. The
    * event type is checked against the regular expressions provided until a 
    * positive match is found or the whole expression list has been traversed
    *
    * @param notification the notification to be checked
   **/
   public boolean isNotificationEnabled(Notification notification)
   {
      synchronized(this.regExps) {
         // Get all existing compiled patterns
         Collection patterns = this.regExps.values();
            
         // Iterate and check matching until positive match found or end of
         // pattern list reached
         Iterator i = patterns.iterator();
         while (i.hasNext()) {
            // Get the pattern
            RE p = (RE)i.next();
                
            // Check matching 
            if (p.isMatch(notification.getType()))
               return true; // Match found. Short circuit search
            else
               ;  // Continue
         }
      }
      return false;
   }
    
} // class RegExpNotificationFilterSupport
