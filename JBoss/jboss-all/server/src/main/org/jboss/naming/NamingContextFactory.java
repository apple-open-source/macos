/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.naming;

import java.util.Hashtable;
import javax.naming.Context;
import javax.naming.NamingException;

/** A variation of the org.jnp.interfaces.NamingContextFactory
 * InitialContextFactory implementation that maintains the last envrionment
 * used to create an InitialContext in a thread local variable for
 * access within the scope of the InitialContext. This can be used by
 * the EJB handles to save the context that should be used to perform the
 * looks when the handle is restored.
 *
 * @see org.jnp.interfaces.NamingContextFactory
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class NamingContextFactory extends org.jnp.interfaces.NamingContextFactory
{
   public static final ThreadLocal lastInitialContextEnv = new ThreadLocal();

   // InitialContextFactory implementation --------------------------
   public Context getInitialContext(Hashtable env)
         throws NamingException
   {
      lastInitialContextEnv.set(env);
      return super.getInitialContext(env);
   }
}
