/*
 * Copyright (c) 2000 Peter Antman DN <peter.antman@dn.se>
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
 */
package org.jboss.jms.jndi;

import java.util.Hashtable;

import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;

import org.jboss.logging.Logger;

/**
 * A JMS provider adapter for <em>JBossMQ</em>.
 *
 * Created: Fri Dec 22 09:34:04 2000
 * 6/22/01 - hchirino - The queue/topic jndi references are now configed via JMX
 *
 * @version <pre>$Revision: 1.9 $</pre>
 * @author  <a href="mailto:peter.antman@dn.se">Peter Antman</a>
 * @author  <a href="mailto:cojonudo14@hotmail.com">Hiram Chirino</a>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class JBossMQProvider
   extends AbstractJMSProviderAdapter
{
   /** The initial context factory to use. */
   public static final String INITIAL_CONTEXT_FACTORY =
      "org.jnp.interfaces.NamingContextFactory";

   /** The url package prefixes. */
   public static final String URL_PKG_PREFIXES =
      "org.jboss.naming";

   /** The security manager to use. */
   private static final String SECURITY_MANAGER =
      "java.naming.rmi.security.manager";

   /** Instance logger. */
   private transient Logger log = Logger.getLogger(this.getClass());

   /** Flag to enable JNDI security manager. */
   private String hasJndiSecurityManager = "yes";

   /**
    * Default no-argument constructor.
    */
   public JBossMQProvider() {
      // empty
      log.debug("initializing");
   }

   /** Override of standard de-serialization to re-create logger. */
   private void readObject(java.io.ObjectInputStream in)
      throws java.io.IOException, ClassNotFoundException
   {
      in.defaultReadObject();
      this.log = Logger.getLogger(this.getClass());
   }

   /**
    * Create a new InitialContext suitable for this JMS provider.
    *
    * @return  An InitialContext suitable for this JMS provider.
    *
    * @throws NamingException  Failed to construct context.
    */
   public Context getInitialContext() throws NamingException {
      Context ctx = null;
      boolean debug = log.isDebugEnabled();
      if (providerURL == null) {
         // Use default
         if (debug)
            log.debug("no provider url; connecting to local JNDI");
         ctx = new InitialContext(); // Only for JBoss embedded now
      } else {
         // Try another location
         Hashtable props = new Hashtable();
         props.put(Context.INITIAL_CONTEXT_FACTORY,
                   INITIAL_CONTEXT_FACTORY);
         props.put(Context.PROVIDER_URL, providerURL);
         props.put(SECURITY_MANAGER, hasJndiSecurityManager);
         props.put(Context.URL_PKG_PREFIXES, URL_PKG_PREFIXES);

         if (debug)
            log.debug("connecting to remote JNDI with props: " + props);
         ctx = new InitialContext(props);
      }

      if (debug) {
         log.debug("created context: " + ctx);
      }
      return ctx;
   }
}
