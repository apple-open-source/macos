/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jmx.interceptors;

import org.jboss.mx.interceptor.AbstractInterceptor;
import org.jboss.mx.interceptor.Invocation;
import org.jboss.mx.interceptor.InvocationException;
import org.jboss.logging.Logger;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public final class SecurityInterceptor
   extends AbstractInterceptor
{
   private static Logger log = Logger.getLogger(SecurityInterceptor.class);

   private String securityDomain;

   public String getSecurityDomain()
   {
      return securityDomain;
   }
   public void setSecurityDomain(String securityDomain)
   {
      log.info("setSecurityDomain: "+securityDomain);
      this.securityDomain = securityDomain;
   }

   // Interceptor overrides -----------------------------------------
   public Object invoke(Invocation invocation) throws InvocationException
   {
      String opName = invocation.getName();
      log.info("invoke, opName="+opName);
      if( opName.startsWith("secret") )
      {
         throw new InvocationException(
               new SecurityException("No secret methods are invocable")
         );
      }
      return getNext().invoke(invocation);
   }
}
