/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jmx.interceptors;

import java.security.Principal;
import org.jboss.mx.interceptor.AbstractInterceptor;
import org.jboss.mx.interceptor.Invocation;
import org.jboss.mx.interceptor.InvocationException;
import org.jboss.logging.Logger;
import org.jboss.security.SecurityAssociation;

/** An interceptor that simply asserts the caller is jduke
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public final class PrincipalInterceptor
   extends AbstractInterceptor
{
   private static Logger log = Logger.getLogger(PrincipalInterceptor.class);

   // Interceptor overrides -----------------------------------------
   public Object invoke(Invocation invocation) throws InvocationException
   {
      Principal caller = SecurityAssociation.getPrincipal();
      String opName = invocation.getName();
      log.info("invoke, opName="+opName+", caller="+caller);
      if( caller == null || caller.getName().equals("jduke") == false )
      {
         throw new InvocationException(
               new SecurityException("Caller="+caller+" is not jduke")
         );
      }
      return getNext().invoke(invocation);
   }
}
