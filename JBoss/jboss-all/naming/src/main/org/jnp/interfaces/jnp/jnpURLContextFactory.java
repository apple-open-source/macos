/*
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jnp.interfaces.jnp;

import java.util.Hashtable;
import javax.naming.*;
import javax.naming.spi.*;

import org.jnp.interfaces.NamingContext;

/** The URL context factory for jnp: style URLs.
 *      
 *   @see <related>
 *   @author oberg
 *   @author Scott_Stark@displayscape.com
 *   @version $Revision: 1.4 $
 */
public class jnpURLContextFactory
   implements ObjectFactory
{
   // Constants -----------------------------------------------------
    
   // Attributes ----------------------------------------------------
    
   // Static --------------------------------------------------------
   
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
      {
          Context urlContext = new NamingContext(environment, name, null);
          return urlContext;
      }
      else if (obj instanceof String)
      {
         String url = (String)obj;
         Context ctx = new NamingContext(environment, name, null);
         
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
