package org.jboss.ejb.plugins.local;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;

/** The EJBLocal proxy for an entity

 @author  <a href="mailto:scott.stark@jboss.org">Scott Stark</a>
 @version $Revision: 1.3 $
 */
class EntityProxy extends LocalProxy
   implements InvocationHandler
{
   static final long serialVersionUID = 5196148608172665115L;
   private Object cacheKey;

   EntityProxy(String jndiName, Object id, BaseLocalProxyFactory factory)
   {
      super(jndiName, factory);
      cacheKey = id;
   }

   protected Object getId()
   {
      return cacheKey;
   }

   public final Object invoke(final Object proxy, final Method m, Object[] args)
      throws Throwable
   {
      if (args == null)
         args = EMPTY_ARGS;

      Object retValue = super.invoke( proxy, m, args );
      if( retValue == null )
      {
         // If not taken care of, go on and call the container
         retValue = factory.invoke(cacheKey, m, args);
      }
      return retValue;
   }

}
