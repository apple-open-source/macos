/*
* JBoss, the OpenSource J2EE WebOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.invocation.http.interfaces;

import java.io.Externalizable;
import java.lang.reflect.Method;

import org.jboss.invocation.Invocation;
import org.jboss.proxy.Interceptor;

/** Handle toString, equals, hashCode locally on the client.
 * 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.2 $
 */
public class ClientMethodInterceptor extends Interceptor
   implements Externalizable
{
   /** The serialVersionUID. @since 1.1.4.1 */
   private static final long serialVersionUID = 3511654206312502958L;

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
      HttpInvokerProxy proxy = (HttpInvokerProxy) mi.getInvocationContext().getInvoker();
      // Implement local methods
      if( methodName.equals("toString") )
      {
         return toString(proxy);
      }
      if( methodName.equals("equals") )
      {
         Object[] args = mi.getArguments();
         String thisString = toString(proxy);
         String argsString = args[0] == null ? "" : args[0].toString();
         return new Boolean(thisString.equals(argsString));
      }
      if( methodName.equals("hashCode") )
      {
         return (Integer) mi.getObjectName();
      }

      return getNext().invoke(mi);
   }

   private String toString(HttpInvokerProxy proxy)
   {
      StringBuffer tmp = new StringBuffer(proxy.toString());
      tmp.append('{');
      tmp.append("externalURLValue="+proxy.getExternalURLValue());
      tmp.append(",externalURL="+proxy.getExternalURL());
      tmp.append('}');
      return tmp.toString();
   }
}
