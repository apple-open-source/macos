/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.system;

import java.net.MalformedURLException;

import java.rmi.server.RMIClassLoader;
import java.rmi.server.RMIClassLoaderSpi;

/**
 * An implementation of RMIClassLoaderSpi to workaround the
 * proxy ClassCastException problem in 1.4<p>
 *
 * <b>THIS IS A HACK!</b><p>
 *
 * Sun's implementation uses the caller classloader when
 * unmarshalling proxies. This is effectively jboss.jar since
 * that is where JRMPInvokerProxy lives. On a redeploy the
 * new interfaces are ignored because a proxy is already cached
 * against the classloader.<p>
 *
 * This class ignores Sun's guess at a suitable classloader and
 * uses the thread context classloader instead.<p>
 *
 * It has to exist in the system classloader so I have included it
 * in "system" for inclusion in run.jar<p>
 * 
 * @author <a href="mailto:adrian.brock@happeningtimes.com">Adrian Brock</a>
 * @version $Revision: 1.1.4.2 $
 */
public class JBossRMIClassLoader
   extends RMIClassLoaderSpi
{
   // Attributes ----------------------------------------------------

   /**
    * The JVM implementation (we delegate most work to it)
    */
   RMIClassLoaderSpi delegate = RMIClassLoader.getDefaultProviderInstance();
   
   // Constructors --------------------------------------------------

   /**
    * Required constructor
    */
   public JBossRMIClassLoader()
   {
   }
   
   // RMIClassLoaderSpi Implementation ------------------------------

   /**
    * Ignore the JVM, use the thread context classloader for proxy caching
    */
   public Class loadProxyClass(String codebase, String[] interfaces, ClassLoader ignored)
      throws MalformedURLException, ClassNotFoundException
   {
      return delegate.loadProxyClass(codebase, interfaces, Thread.currentThread().getContextClassLoader());
   }

   /**
    * Just delegate
    */
   public Class loadClass(String codebase, String name, ClassLoader ignored)
      throws MalformedURLException, ClassNotFoundException
   {
      return delegate.loadClass(codebase, name, Thread.currentThread().getContextClassLoader());
   }

   /**
    * Just delegate
    */
   public ClassLoader getClassLoader(String codebase)
      throws MalformedURLException
   {
      return delegate.getClassLoader(codebase);
   }

   /**
    * Just delegate
    */
   public String getClassAnnotation(Class cl)
   {
      return delegate.getClassAnnotation(cl);
   }
}
