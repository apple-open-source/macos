/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.naming.interceptors;

import javax.naming.InitialContext;
import org.jboss.mx.interceptor.AbstractInterceptor;
import org.jboss.mx.interceptor.Invocation;
import org.jboss.mx.interceptor.InvocationException;
import org.jboss.logging.Logger;
import org.jnp.interfaces.NamingContext;
import org.jnp.interfaces.Naming;

/** An interceptor that replaces any NamingContext values returned with the
 * proxy found under the JNDI binding given by the proxyName.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class ProxyFactoryInterceptor
   extends AbstractInterceptor
{
   private static Logger log = Logger.getLogger(ProxyFactoryInterceptor.class);
   private String proxyName;
   private Naming proxy;

   public void setProxyName(String proxyName)
   {
      this.proxyName = proxyName;
   }

   // Interceptor overrides -----------------------------------------
   public Object invoke(Invocation invocation) throws InvocationException
   {
      String opName = invocation.getName();
      log.info("invoke, opName="+opName);
      Object value = getNext().invoke(invocation);
      if( value instanceof NamingContext )
      {
         initNamingProxy();
         NamingContext ctx = (NamingContext) value;
         ctx.setNaming(proxy);
      }
      return value;
   }

   private void initNamingProxy()
      throws InvocationException
   {
      if( proxy != null )
         return;

      try
      {
         InitialContext ctx = new InitialContext();
         proxy = (Naming) ctx.lookup(proxyName);
      }
      catch(Exception e)
      {
         log.error("Failed to lookup: "+proxyName, e);
         throw new InvocationException(e);
      }
   }
}
