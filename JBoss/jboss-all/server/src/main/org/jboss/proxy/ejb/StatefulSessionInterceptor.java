/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.proxy.ejb;

import java.lang.reflect.Method;

import org.jboss.invocation.Invocation;
import org.jboss.invocation.InvocationContext;
import org.jboss.invocation.InvocationKey;
import org.jboss.invocation.InvocationType;
import org.jboss.invocation.Invoker;
import org.jboss.proxy.ejb.handle.StatefulHandleImpl;

/**
 *
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @version $Revision: 1.5.2.1 $
 */
public class StatefulSessionInterceptor
   extends GenericEJBInterceptor
{
   /** Serial Version Identifier. @since 1.5 */
   private static final long serialVersionUID = -4333233488946091285L;

   /**
    * No-argument constructor for externalization.
    */
   public StatefulSessionInterceptor()
   {
   }

   // Public --------------------------------------------------------
   
   /**
    * InvocationHandler implementation.
    *
    * @throws Throwable    Any exception or error thrown while processing.
    */
   public Object invoke(Invocation invocation)
      throws Throwable
   {
      InvocationContext ctx = invocation.getInvocationContext();
      
      Method m = invocation.getMethod();
      
      // Implement local methods
      if (m.equals(TO_STRING))
      {
         return toString(ctx);
      }
      else if (m.equals(EQUALS))
      {
         Object[] args = invocation.getArguments();
         String argsString = args[0] != null ? args[0].toString() : "";
         String thisString = toString(ctx);
         return new Boolean(thisString.equals(argsString));
      }
      else if (m.equals(HASH_CODE))
      {
         return new Integer(ctx.getCacheId().hashCode());
      }
      // Implement local EJB calls
      else if (m.equals(GET_HANDLE))
      {
         int objectName = ((Integer) ctx.getObjectName()).intValue();
         String jndiName = (String) ctx.getValue(InvocationKey.JNDI_NAME);
         Invoker invoker = ctx.getInvoker();
         Object id = ctx.getCacheId();
         return new StatefulHandleImpl(
               objectName, 
               jndiName, 
               invoker, 
               ctx.getInvokerProxyBinding(), 
               id);
      }
      else if (m.equals(GET_EJB_HOME))
      {
         return getEJBHome(invocation);
      }
      else if (m.equals(GET_PRIMARY_KEY))
      {
         return ctx.getCacheId();
      }
      else if (m.equals(IS_IDENTICAL))
      {
         Object[] args = invocation.getArguments();
         String argsString = args[0].toString();
         String thisString = toString(ctx);
         return new Boolean(thisString.equals(argsString));
      }
      // If not taken care of, go on and call the container
      else
      {
         // It is a remote invocation
         invocation.setType(InvocationType.REMOTE);
         
         // On this entry in cache
         invocation.setId(ctx.getCacheId());
         
         return getNext().invoke(invocation);
      }
   }

   private String toString(InvocationContext ctx)
   {
      return ctx.getValue(InvocationKey.JNDI_NAME) + ":" + 
            ctx.getCacheId().toString();
   }
}
