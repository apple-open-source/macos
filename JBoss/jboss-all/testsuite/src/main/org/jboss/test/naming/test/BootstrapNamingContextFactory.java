/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.naming.test;

import java.util.Hashtable;
import javax.naming.Context;
import javax.naming.NamingException;
import javax.naming.InitialContext;
import javax.naming.spi.InitialContextFactory;

import org.jnp.interfaces.Naming;
import org.jnp.interfaces.NamingContext;
import org.jboss.logging.Logger;

/** A naming provider InitialContextFactory implementation that obtains a
 Naming proxy from a JNDI binding using the default InitialContext. This
 is only useful for testing secondary naming services.

 @see InitialContextFactory

 @author Scott.Stark@jboss.org
 @version $Revision: 1.1.2.1 $
 */
public class BootstrapNamingContextFactory
   implements InitialContextFactory
{
   static Logger log = Logger.getLogger(BootstrapNamingContextFactory.class);

   // InitialContextFactory implementation --------------------------
   public Context getInitialContext(Hashtable env)
      throws NamingException
   {
      Naming namingServer = null;
      try
      {
         Hashtable env2 = (Hashtable) env.clone();
         env2.put(Context.INITIAL_CONTEXT_FACTORY, "org.jnp.interfaces.NamingContextFactory");
         // Retrieve the Naming interface
         String location = (String) env.get("bootstrap-binding");
         namingServer = (Naming) new InitialContext(env2).lookup(location);
         log.debug("Found naming proxy:"+namingServer);
      }
      catch(Exception e)
      {
         log.debug("Lookup failed", e);
         NamingException ex = new NamingException("Failed to retrieve Naming interface");
         ex.setRootCause(e);
         throw ex;
      }

      // Copy the context env
      env = (Hashtable) env.clone();
      return new NamingContext(env, null, namingServer);
   }
}
