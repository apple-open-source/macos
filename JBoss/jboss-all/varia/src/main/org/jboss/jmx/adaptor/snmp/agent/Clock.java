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

/** 
 * Utility class that provides time keeping from the time of instantiation.
 * Can be queried for time readings since the last incarnation in msecs.
 *
 * @version $Revision: 1.1.2.1 $
 *
 * @author  <a href="mailto:spol@intracom.gr">Spyros Pollatos</a>
 * @author  <a href="mailto:andd@intracom.gr">Dimitris Andreadis</a>
**/    
public final class Clock
{
   /** Holds instantiation time */
   private long instantiationTime;
          
   /**
    * CTOR - creates time counter with zeroed out clock
   **/          
   public Clock()
   {
        this.instantiationTime = System.currentTimeMillis();
   }

   /**
    * Returns the difference, measured in milliseconds, between the 
    * instantiation time and midnight, January 1, 1970 UTC.
   **/                         
   public long instantiationTime()
   {
      return this.instantiationTime;
   }
    
   /**
    * Returns the elapsed time in msecs from the time of instantiation.
   **/                         
   public long uptime()
   {
      return System.currentTimeMillis() - this.instantiationTime;
   }  

} // class Clock
