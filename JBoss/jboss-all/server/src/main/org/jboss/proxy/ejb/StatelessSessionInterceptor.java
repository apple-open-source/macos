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
import org.jboss.proxy.ejb.handle.StatelessHandleImpl;


/**
 * An EJB stateless session bean proxy class.
 *
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @version $Revision: 1.4.2.1 $
 */
public class StatelessSessionInterceptor
   extends GenericEJBInterceptor
{
   /** Serial Version Identifier. @since 1.4 */
   private static final long serialVersionUID = -8807189798153718350L;

   /**
    * No-argument constructor for externalization.
    */
   public StatelessSessionInterceptor()
   {}
   
   
   // Public --------------------------------------------------------
   
   /**
    * InvocationHandler implementation.
    *
    * @param proxy   The proxy object.
    * @param m       The method being invoked.
    * @param args    The arguments for the method.
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
         // We base the stateless hash on the hash of the proxy...
         // MF XXX: it could be that we want to return the hash of the name?
         return new Integer(this.hashCode());
      }
      // Implement local EJB calls
      else if (m.equals(GET_HANDLE))
      {
         return new StatelessHandleImpl(
               (String)ctx.getValue(InvocationKey.JNDI_NAME));
      }
      else if (m.equals(GET_PRIMARY_KEY))
      {  
         return ctx.getValue(InvocationKey.JNDI_NAME);
      }
      else if (m.equals(GET_EJB_HOME))
      {
         return getEJBHome(invocation);
      }
      else if (m.equals(IS_IDENTICAL))
      {
         // All stateless beans are identical within a home,
         // if the names are equal we are equal
         Object[] args = invocation.getArguments();
         String argsString = args[0].toString();
         String thisString = toString(ctx);
         return new Boolean(thisString.equals(argsString));
      }
      // If not taken care of, go on and call the container
      else
      {
         invocation.setType(InvocationType.REMOTE);
         
         return getNext().invoke(invocation);
      }
   }
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Private -------------------------------------------------------
   private String toString(InvocationContext ctx)
   {
      return ctx.getValue(InvocationKey.JNDI_NAME) + ":Stateless";
   }   
}
