/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.invocation;

import java.io.Externalizable;
import java.io.IOException;
import java.io.ObjectInput;
import java.io.ObjectOutput;
import java.lang.reflect.UndeclaredThrowableException;

/**
* An InvokerInterceptor that does not optimize in VM invocations
*
* @author Scott.Stark@jboss.org
* @version $Revision: 1.1.2.2 $
*/
public class MarshallingInvokerInterceptor
   extends InvokerInterceptor
{
   /** Serial Version Identifier. @since 1.1.4.1 */
   private static final long serialVersionUID = -6473336704093435358L;

   public MarshallingInvokerInterceptor()
   {
      // For externalization to work
   }
   
   // Public --------------------------------------------------------

   /**
    * Invoke using the invoker for remote invocations
    */
   public Object invoke(Invocation invocation)
      throws Exception
   {
      Object rtnValue = null;
      if( isLocal() )
      {
         // Enforce by-value call in the same call thread
         MarshalledInvocation mi = new MarshalledInvocation(invocation);
         MarshalledValue copy = new MarshalledValue(mi);
         Invocation invocationCopy = (Invocation) copy.get();
         try
         {
            rtnValue = localInvoker.invoke(invocationCopy);
            MarshalledValue mv = new MarshalledValue(rtnValue);
            rtnValue = mv.get();
         }
         catch(Throwable t)
         {
            MarshalledValue mv = new MarshalledValue(t);
            Throwable t2 = (Throwable) mv.get();
            if( t2 instanceof Exception )
               throw (Exception) t2;
            else
               throw new UndeclaredThrowableException(t2);
         }
      }
      else
      {
         // Go through the transport invoker
         Invoker invoker = invocation.getInvocationContext().getInvoker();
         rtnValue = invoker.invoke(invocation);
      }
      return rtnValue;
   }
}
