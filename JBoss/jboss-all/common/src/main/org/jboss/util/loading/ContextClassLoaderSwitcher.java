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

import org.jboss.logging.Logger;

/**
 * A helper for context classloading switching.<p>
 *
 * When a security manager is installed, the
 * constructor checks for the runtime permissions
 * &quot;getClassLoader&quot; and &quot;setContextClassLoader&quot;.
 * This allows the methods of this class to be used later without
 * having to run in privileged blocks.
 *
 * There are optimized methods to perform the operations within
 * a switch context. This avoids retrieving the current thread
 * on every operation.
 *
 * @version <tt>$Revision: 1.4 $</tt>
 * @author  <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 */
public class ContextClassLoaderSwitcher
   extends ContextClassLoader
{
   /**
    * Set the context classloader permission
    */
   public static final RuntimePermission SETCONTEXTCLASSLOADER = new RuntimePermission("setContextClassLoader");

   /**
    * Instantiate a new context class loader switcher
    */
   public static final NewInstance INSTANTIATOR = new NewInstance();

   /**
    * Constructor.
    * @throws SecurityException when not authroized to get/set the context classloader
    */
   private ContextClassLoaderSwitcher()
   {
      super();
      SecurityManager manager = System.getSecurityManager();
      if (manager != null)
         manager.checkPermission(SETCONTEXTCLASSLOADER);
   }

   /**
    * Set the context classloader
    *
    * @param the new context classloader
    */
   public void setContextClassLoader(final ClassLoader cl)
   {
      setContextClassLoader(Thread.currentThread(), cl);
   }

   /**
    * Set the context classloader for the given thread
    *
    * @param thread the thread
    * @param the new context classloader
    */
   public void setContextClassLoader(final Thread thread, final ClassLoader cl)
   {
      AccessController.doPrivileged(new PrivilegedAction()
      {
         public Object run()
         {
            thread.setContextClassLoader(cl);
            return null;
         }
      });
   }

   /**
    * Retrieve a switch context
    *
    * @return the switch context
    */
   public SwitchContext getSwitchContext()
   {
      return new SwitchContext();
   }

   /**
    * Retrieve a switch context and set the new context classloader
    *
    * @param cl the new classloader
    * @return the switch context
    */
   public SwitchContext getSwitchContext(final ClassLoader cl)
   {
      return new SwitchContext(cl);
   }

   /**
    * Retrieve a switch context for the classloader of a given class
    *
    * @deprecated using a class to determine the classloader is a 
    *             bad idea, it has the same problems as Class.forName()
    * @param clazz the class whose classloader should be set
    *        as the context classloader
    * @return the switch context
    */
   public SwitchContext getSwitchContext(final Class clazz)
   {
      return new SwitchContext(clazz.getClassLoader());
   }

   /**
    * A helper class to remember the original classloader and
    * avoid continually retrieveing the current thread.
    */
   public class SwitchContext
   {
      /**
       * The original classloader
       */
      private ClassLoader origCL;

      /**
       * The current classloader
       */
      private ClassLoader currentCL;

      /**
       * The current thread
       */
      private Thread currentThread;

      private SwitchContext()
      {
         currentThread = Thread.currentThread();
         origCL = getContextClassLoader(currentThread);
         currentCL = origCL;
      }

      private SwitchContext(ClassLoader cl)
      {
         this();
         setClassLoader(cl);
      }

      /**
       * Retrieve the current thread
       */
      public Thread getThread()
      {
         return currentThread;
      }

      /**
       * Retrieve the original classloader
       */
      public ClassLoader getOriginalClassLoader()
      {
         return origCL;
      }

      /**
       * Retrieve the current classloader
       * (as set through this class).
       */
      public ClassLoader getCurrentClassLoader()
      {
         return currentCL;
      }

      /**
       * Change the context classloader<p>
       *
       * The operation is ignored if the classloader is null
       * or has not changed
       *
       * @param cl the new classloader
       */
      public void setClassLoader(ClassLoader cl)
      {
         if (cl != null && cl != currentCL)
         {
            setContextClassLoader(currentThread, cl);
            currentCL = cl;
         }
      }

      /**
       * Reset back to the original classloader,
       * only when it has changed.
       */
      public void reset()
      {
         if (currentCL != null && currentCL != origCL)
            setContextClassLoader(currentThread, origCL);
      }

      /**
       * Force a reset back to the original classloader,
       * useful when somebody else might have changed
       * the thread context classloader so we cannot optimize
       */
      public void forceReset()
      {
         setContextClassLoader(currentThread, origCL);
      }
   }

   private static class NewInstance
      implements PrivilegedAction
   {
      public Object run()
      {
         return new ContextClassLoaderSwitcher();
      }
   }
}
