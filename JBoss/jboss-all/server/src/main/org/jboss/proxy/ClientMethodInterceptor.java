/*
* JBoss, the OpenSource J2EE WebOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.proxy;

import java.io.Externalizable;
import java.lang.reflect.Method;

import org.jboss.invocation.Invocation;
import org.jboss.invocation.Invoker;
import org.jboss.proxy.Interceptor;

/** Handle toString, equals, hashCode locally on the client.
 * 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class ClientMethodInterceptor extends Interceptor
   implements Externalizable
{
   /** The serialVersionUID. @since 1.1.2.1 */
   private static final long serialVersionUID = 6010013004557885014L;

   /** Handle methods locally on the client
    *
    * @param mi
    * @return
    * @throws Throwable
    */
   public Object invoke(Invocation mi) throws Throwable
   {
      Method m = mi.getMethod();
      String methodName = m.getName();
      Invoker proxy = mi.getInvocationContext().getInvoker();
      // Implement local methods
      if( methodName.equals("toString") )
      {
         return proxy.toString();
      }
      if( methodName.equals("equals") )
      {
         Object[] args = mi.getArguments();
         String thisString = proxy.toString();
         String argsString = args[0] == null ? "" : args[0].toString();
         return new Boolean(thisString.equals(argsString));
      }
      if( methodName.equals("hashCode") )
      {
         return (Integer) mi.getObjectName();
      }

      return getNext().invoke(mi);
   }

}
