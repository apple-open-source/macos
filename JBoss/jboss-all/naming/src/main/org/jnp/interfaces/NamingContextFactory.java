/*
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jnp.interfaces;

import java.util.Hashtable;
import javax.naming.CompoundName;
import javax.naming.Context;
import javax.naming.Name;
import javax.naming.NamingException;
import javax.naming.Reference;
import javax.naming.spi.*;

/** The jnp naming provider InitialContextFactory implementation.

@see javax.naming.spi.InitialContextFactory

@author Scott.Stark@jboss.org
@version $Revision: 1.3.10.2 $
 */
public class NamingContextFactory
   implements InitialContextFactory, ObjectFactory
{
    // InitialContextFactory implementation --------------------------
    public Context getInitialContext(Hashtable env) 
      throws NamingException
    {
        String providerURL = (String) env.get(Context.PROVIDER_URL);
        Name prefix = null;
        /** This may be a comma separated list of provider urls in which
          case we do not parse the urls for the requested context prefix name
        */
        int comma = providerURL != null ? providerURL.indexOf(',') : -1;
        if( providerURL != null && comma < 0 )
        {
            Name name = new CompoundName(providerURL, NamingParser.syntax);
            String serverInfo = NamingContext.parseNameForScheme(name);
            if( serverInfo != null )
            {
               env = (Hashtable) env.clone();
               // Set hostname:port value for the naming server
               env.put(Context.PROVIDER_URL, serverInfo);
               // Set the context prefix to name
               prefix = name;
            }
        }
        return new NamingContext(env, prefix, null);
    }

   // ObjectFactory implementation ----------------------------------
   public Object getObjectInstance(Object obj,
                                Name name,
                                Context nameCtx,
                                Hashtable environment)
                         throws Exception
   {
//      System.out.println(obj+" "+name+" "+nameCtx+" "+environment);
      Context ctx = getInitialContext(environment);
      Reference ref = (Reference)obj;
      return ctx.lookup((String)ref.get("URL").getContent());
   }

}
