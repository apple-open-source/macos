/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.local;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import javax.ejb.EJBLocalHome;
import javax.ejb.EJBLocalObject;
import javax.ejb.Handle;
import javax.ejb.HomeHandle;
import javax.naming.Name;

/** The proxy for an EJBLocalHome object.

 @author <a href="mailto:docodan@mvcsoft.com">Daniel OConnor</a>
 @author <a href="mailto:scott.stark@jboss.org">Scott Stark</a>
 @version $Revision: 1.7 $
 */
public class LocalHomeProxy
   extends LocalProxy
   implements InvocationHandler
{
   static final long serialVersionUID = 1762319499924478521L;

   /** {@link EJBHome#remove(Object)} method reference. */
   protected static final Method REMOVE_BY_PRIMARY_KEY;
   
   /** {@link EJBObject#remove} method reference. */
   protected static final Method REMOVE_OBJECT;
   
   /**
    * Initialize {@link EJBHome} and {@link EJBObject} method references.
    */
   static
   {
      try
      {
         final Class empty[] = {};
         final Class type = EJBLocalHome.class;
         
         REMOVE_BY_PRIMARY_KEY = type.getMethod("remove", new Class[] {Object.class});
         
         // Get the "remove" method from the EJBObject
         REMOVE_OBJECT = EJBLocalObject.class.getMethod("remove", empty);
      }
      catch (Exception e)
      {
         e.printStackTrace();
         throw new ExceptionInInitializerError(e);
      }
   }
   
   public LocalHomeProxy(String jndiName, BaseLocalProxyFactory factory)
   {
      super(jndiName, factory);
   }

   protected Object getId()
   {
      return jndiName;
   }

   /**
    * InvocationHandler implementation.
    *
    * @param proxy   The proxy object.
    * @param m       The method being invoked.
    * @param args    The arguments for the method.
    *
    * @throws Throwable    Any exception or error thrown while processing.
    */
   public Object invoke(final Object proxy, final Method m,
      Object[] args)
      throws Throwable
   {
      Object retValue = null;

      if (args == null)
         args = EMPTY_ARGS;

      // Implement local methods
      if (m.equals(TO_STRING))
      {
         retValue = jndiName + "Home";
      }
      else if (m.equals(EQUALS))
      {
         // equality of the proxy home is based on names...
         Object temp = invoke(proxy, TO_STRING, args);
         retValue = new Boolean(temp.equals(jndiName + "Home"));
      }
      else if (m.equals(HASH_CODE))
      {
         retValue = new Integer(this.hashCode());
      }
      else if (m.equals(REMOVE_BY_PRIMARY_KEY))
      {
         // The trick is simple we trick the container in believe it
         // is a remove() on the instance
         Object id = args[0];
         retValue = factory.invoke(id, REMOVE_OBJECT, EMPTY_ARGS);
      }
      // If not taken care of, go on and call the container
      else
      {
         retValue = factory.invokeHome(m, args);
      }

      return retValue;
   }
}
