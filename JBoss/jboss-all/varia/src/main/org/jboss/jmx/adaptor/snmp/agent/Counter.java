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
 * Simple, thread safe counter implementattion with accessor methods.
 *
 * (Maybe replace with EDU.oswego.cs.dl.util.concurrent.SynchronizedLong?)
 *
 * @version $Revision: 1.1.2.1 $
 *
 * @author  <a href="mailto:spol@intracom.gr">Spyros Pollatos</a>
 * @author  <a href="mailto:andd@intracom.gr">Dimitris Andreadis</a>
**/
public final class Counter
{
   /** the internal counter */
   private long count;
   
   /**
    * CTOR - initialises the counter to the provided value.
    *
    * @param countStart the starting counter value
   **/        
   public Counter(long countStart)
   {
      this.count = countStart;
   }
     
   /**
    * Returns the current value (i.e. the next to be used).
   **/                 
   public synchronized long peek()
   {
      return this.count;
   }
     
   /**
    * Returns the current value (i.e. the next to be used) and advances
    * the counter by one.
   **/                 
   public synchronized long advance()
   {
      return this.count++;
   }
        
} // class Counter
