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
import java.util.Date;
import javax.management.Notification;

/**
 * <tt>NotificationWrapperSupport</tt> provides a base 
 * NotificationWrapper implementation
 *
 * @version $Revision: 1.1.2.1 $
 *
 * @author  <a href="mailto:spol@intracom.gr">Spyros Pollatos</a>
 * @author  <a href="mailto:andd@intracom.gr">Dimitris Andreadis</a>
**/
public class NotificationWrapperSupport
   implements NotificationWrapper
{
    /** Holds the notification payload keyed on the attibute name */
    protected Map payload = new HashMap();
    
    /** Provides uptime */
    protected Clock clock;
    
    /** Provides trap count */
    protected Counter trapCount;

   /**
    * CTOR
   **/
   public NotificationWrapperSupport()
   {
      // empty
   }
   
   /**
    * Loads the hashmap with the DCAs of interest. Note that the keys are used 
    * as attribute tags in the mapping resource file 
   **/
   public void set(Clock uptime, Counter count)
   {
      this.clock = uptime;
      this.trapCount = count;
        
      this.payload.put(STARTTIME_TAG,
                       new Date(this.clock.instantiationTime()));
      
      this.payload.put(UPTIME_TAG, // anonymous class
           new DynamicContentAccessor() {
              public Object get()
              {
                 return new Long(NotificationWrapperSupport.this.clock.uptime());
              }
           });

      this.payload.put(TRAPCOUNT_TAG, // anonymous class
           new DynamicContentAccessor() {
              public Object get()
              {
                 return new Long(NotificationWrapperSupport.this.trapCount.peek());
              }
           });
   }

   /**
    * Set the notification to be used as the data source. Load the hashmap 
    * with all of the notification contents. Note that the keys are used 
    * as attribute tags in the mapping resource file 
    *
    * @param n the notification to be used as data source at subsequent calls
    *          of get()
   **/    
   public void prime(Notification n)
   {
      // Get fixed event payload and general info
      this.payload.put(MESSAGE_TAG, n.getMessage());
      this.payload.put(SEQNO_TAG, new Long(n.getSequenceNumber()));
      this.payload.put(TSTAMP_TAG, new Long(n.getTimeStamp()));
      this.payload.put(TYPE_TAG, n.getType());
      this.payload.put(ALL_TAG, n.toString());
      this.payload.put(CLASS_TAG, n.getClass().getName());

      // Check if event contains anything in the user field. If there is, an
      // attempt is made to interpret it as a hash map and copy it. Note 
      // that previous content may be overwritten if the same keys as above 
      // are used
      Object userData = n.getUserData();
      if (userData instanceof HashMap) {
         // Copy all of the user data in the payload
         this.payload.putAll((HashMap)userData);    
      }
   } // prime

   /**
    * Implements the communication protocol between the caller and the data 
    * source (notification and agent) based on tags. Implementations are 
    * expected to map the provided attribute name to some aspect of the 
    * notification payload. The later is defined by method prime.
    *
    * @param tagName the tag of the attribute the value of which is required
   **/    
   public Object get(String tagName)
      throws MappingFailedException
   {
      Object o = this.payload.get(tagName); 
       
      if (o == null)
         throw new MappingFailedException("Tag \"" + tagName + "\" not found");
       
      // Check whether value returned is a dynamic content accessor. If not
      // return as is. If yes invoke the accessor and return that value
      if (o instanceof DynamicContentAccessor) {
         DynamicContentAccessor d = (DynamicContentAccessor)o;

         return d.get();     
      }
      else {
         return o;
      }
   } //get

} // NotificationWrapperSupport
