/*
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jnp.interfaces.java;

import java.util.Hashtable;
import javax.naming.*;
import javax.naming.spi.*;

import org.jnp.interfaces.NamingContext;
import org.jnp.interfaces.Naming;

/**
 *   Implementation of "java:" namespace factory. The context is associated
 *   with the thread, so the root context must be set before this is used in a thread
 *      
 *   @see <related>
 *   @author $Author: oberg $
 *   @version $Revision: 1.2 $
 */
public class javaURLContextFactory
   implements ObjectFactory
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
    
   // Static --------------------------------------------------------
   private static ThreadLocal server = new ThreadLocal();
   
   public static void setRoot(Naming srv)
   {
      // TODO: Add security check here
      server.set(srv);
   }
   
   public static Naming getRoot()
   {
      // TODO: Add security check here
      return (Naming)server.get();
   }
   
   // Constructors --------------------------------------------------
   
   // Public --------------------------------------------------------

   // ObjectFactory implementation ----------------------------------
   public Object getObjectInstance(Object obj,
                                Name name,
                                Context nameCtx,
                                Hashtable environment)
                         throws Exception
   {
      if (obj == null)
         return new NamingContext(environment, name, (Naming)server.get());
      else if (obj instanceof String)
      {
         String url = (String)obj;
         Context ctx = new NamingContext(environment, name, (Naming)server.get());
         
         Name n = ctx.getNameParser(name).parse(url.substring(url.indexOf(":")+1));
         if (n.size() >= 3)
         {
            // Provider URL?
            if (n.get(0).toString().equals("") &&
                n.get(1).toString().equals(""))
            {
               ctx.addToEnvironment(Context.PROVIDER_URL, n.get(2));
            }
         }
         return ctx;
      } else
      {
         return null;
      }
   }
    
   // Y overrides ---------------------------------------------------

   // Package protected ---------------------------------------------
    
   // Protected -----------------------------------------------------
    
   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}