/*
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jnp.interfaces;

import java.util.Collection;
import java.util.Iterator;

import javax.naming.NamingEnumeration;

/**
 *   <description> 
 *      
 *   @see <related>
 *   @author $Author: oberg $
 *   @version $Revision: 1.2 $
 */
public class NamingEnumerationImpl
   implements NamingEnumeration
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
   Iterator enum;
    
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   NamingEnumerationImpl(Collection list)
   {
      enum = list.iterator();
   }
   
   // Public --------------------------------------------------------

   // Enumeration implementation ------------------------------------
   public boolean hasMoreElements()
   {
      return enum.hasNext();
   }
   
   public Object nextElement()
   {
      return enum.next();
   }

   // NamingEnumeration implementation ------------------------------
   public boolean hasMore()
   {
      return enum.hasNext();
   }
   
   public Object next()
   {
      return enum.next();
   }
   
   public void close()
   {
      enum = null;
   }

   // Y overrides ---------------------------------------------------

   // Package protected ---------------------------------------------
    
   // Protected -----------------------------------------------------
    
   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}