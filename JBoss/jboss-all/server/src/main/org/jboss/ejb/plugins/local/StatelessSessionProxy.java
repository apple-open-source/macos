package org.jboss.ejb.plugins.local;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import javax.naming.InitialContext;


/** The EJBLocal proxy for a stateless session

 @author  <a href="mailto:scott.stark@jboss.org">Scott Stark</a>
 @version $Revision: 1.3.2.1 $
 */
class StatelessSessionProxy extends LocalProxy
   implements InvocationHandler
{
   static final long serialVersionUID = 5677941766264344565L;

   StatelessSessionProxy(String jndiName, BaseLocalProxyFactory factory)
   {
      super(jndiName, factory);
   }
   
   protected Object getId()
   {
      return jndiName;
   }
   
   public final Object invoke(final Object proxy, final Method m, Object[] args)
      throws Throwable
   {
      Object retValue = null;
      if (args == null)
         args = EMPTY_ARGS;
      
      // Implement local methods
      if (m.equals(TO_STRING))
      {
         retValue = jndiName + ":Stateless";
      }
      else if (m.equals(EQUALS))
      {
         retValue = invoke(proxy, IS_IDENTICAL, args);
      }
      else if (m.equals(HASH_CODE))
      {
         // We base the stateless hash on the hash of the proxy...
         // MF XXX: it could be that we want to return the hash of the name?
         retValue = new Integer(this.hashCode());
      }
      else if (m.equals(GET_PRIMARY_KEY))
      {
         // MF FIXME
         // The spec says that SSB PrimaryKeys should not be returned and the call should throw an exception
         // However we need to expose the field *somehow* so we can check for "isIdentical"
         // For now we use a non-spec compliant implementation and just return the key as is
         // See jboss1.0 for the PKHolder and the hack to be spec-compliant and yet solve the problem
         
         // This should be the following call
         //throw new RemoteException("Session Beans do not expose their keys, RTFS");
         
         // This is how it can be solved with a PKHolder (extends RemoteException)
         // throw new PKHolder("RTFS", name);
         
         // This is non-spec compliant but will do for now
         // We can consider the name of the container to be the primary key, since all stateless beans
         // are equal within a home
         retValue = jndiName;
      }
      else if (m.equals(GET_EJB_HOME))
      {
         InitialContext ctx = new InitialContext();
         return ctx.lookup(jndiName);
      }
      else if (m.equals(IS_IDENTICAL))
      {
         // All stateless beans are identical within a home,
         // if the names are equal we are equal
         retValue = isIdentical(args[0], jndiName);
      }
      // If not taken care of, go on and call the container
      else
      {
         retValue = factory.invoke(jndiName, m, args);
      }

      return retValue;
   }
}
