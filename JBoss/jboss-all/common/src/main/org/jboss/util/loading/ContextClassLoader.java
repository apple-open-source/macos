/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.loading;

import java.security.AccessController;
import java.security.PrivilegedAction;

/**
 * A helper for context classloading.<p>
 *
 * When a security manager is installed, the
 * constructor checks for the runtime permissions
 * &quot;getClassLoader&quot;
 *
 * @version <tt>$Revision: 1.3 $</tt>
 * @author  <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 */
public class ContextClassLoader
{
   /**
    * Retrieve a classloader permission
    */
   public static final RuntimePermission GETCLASSLOADER = new RuntimePermission("getClassLoader");

   /**
    * Instantiate a new context class loader
    */
   public static final NewInstance INSTANTIATOR = new NewInstance();

   /**
    * Constructor.
    * 
    * @throws SecurityException when not authroized to get the context classloader
    */
   /*package*/ ContextClassLoader()
   {
      SecurityManager manager = System.getSecurityManager();
      if (manager != null)
      {
         manager.checkPermission(GETCLASSLOADER);
      }
   }

   /**
    * Retrieve the context classloader
    *
    * @return the context classloader
    */
   public ClassLoader getContextClassLoader()
   {
      return getContextClassLoader(Thread.currentThread());
   }

   /**
    * Retrieve the context classloader for the given thread
    *
    * @param thread the thread
    * @return the context classloader
    */
   public ClassLoader getContextClassLoader(final Thread thread)
   {
      return (ClassLoader) AccessController.doPrivileged(new PrivilegedAction()
      {
         public Object run()
         {
            return thread.getContextClassLoader();
         }
      });
   }

   private static class NewInstance
      implements PrivilegedAction
   {
      public Object run()
      {
         return new ContextClassLoader();
      }
   }
}
