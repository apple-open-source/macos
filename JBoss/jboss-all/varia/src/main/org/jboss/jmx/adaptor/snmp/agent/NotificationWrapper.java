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

import javax.management.Notification;

/**
 * <tt>NotificationWrapper</tt> is a "wide" read only interface 
 * providing "unstructured" access to agent internals and paylod carried by 
 * notifications.
 *
 * The alternatives were:
 *
 * 1.   Precompile assumptions regarding the notification stucture and its 
 *      payload in the trap factory
 * 2.   Use introspection but with a potential large execution overhead. 
 *
 * The use of delegated wrappers stands somewhere in the middle. Trap
 * factory can be extended with the definition and use of new wpappers
 * that will be able to cope with any kind of notfications. Run time overhead
 * is minimal as wrappers can be instantiated only once.
 *
 * Tags used to locate attributes in the notification fixed (i.e. excluding
 * user defined fields) payload are defined. Implementations should not use 
 * these tags in any of the notification payload. To avoid conflicts the 
 * following convention is proposed: standard notification payload is qualified 
 * by the "n:" prefix. Agent properties are qualified by the "a:" prefix. User 
 * defined payload can be qualified with e.g. the "u:" prefix
 *
 * @version $Revision: 1.1.2.1 $
 *
 * @author  <a href="mailto:spol@intracom.gr">Spyros Pollatos</a>
 * @author  <a href="mailto:andd@intracom.gr">Dimitris Andreadis</a>
**/
public interface NotificationWrapper
{
   // Notification content
   public static final String MESSAGE_TAG = "n:message";
   public static final String SEQNO_TAG   = "n:sequenceNumber";
   public static final String TSTAMP_TAG  = "n:timeStamp";
   public static final String TYPE_TAG    = "n:type";
   public static final String ALL_TAG     = "n:all";
   public static final String CLASS_TAG   = "n:class";
    
   // Agent properties
   public static final String STARTTIME_TAG = "a:startTime";
   public static final String UPTIME_TAG    = "a:uptime";
   public static final String TRAPCOUNT_TAG = "a:trapCount";

   /**
    * Sets the uptime clock and trap counter
   **/
   public void set(Clock uptime, Counter count);
   
   /**
    * Sets notification to be used as the data source
    *
    * @param n the notification to be used as a data source at subsequent calls
    *          of method get
   **/ 
   public void prime(Notification n);        
    
   /**
    * Defines the communication protocol between the caller and the data 
    * source (notification). Implementations are expected to map the provided 
    * attribute name to some aspect of the notification payload. The later is
    * defined by method prime.
    *
    * @param attrTag the tag of the attribute the value of which is required
    * @return the content attrTag maps to
    * @throws MappingFailedException if for any reason the requested attribute
    *         can not be found
   **/    
   public Object get(String attrTag) 
       throws MappingFailedException;
    
} // interface NotificationWrapper
