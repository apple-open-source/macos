/***************************************
*                                     *
*  JBoss: The OpenSource J2EE WebOS   *
*                                     *
*  Distributable under LGPL license.  *
*  See terms of license at gnu.org.   *
*                                     *
***************************************/

package org.jboss.mx.loading;

import java.io.InputStream;
import java.net.URL;
import java.net.URLClassLoader;
import java.security.CodeSource;
import java.security.PermissionCollection;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.jboss.logging.Logger;

/** An extension of UnifiedClassLoader that only allows a single thread
 into its UnifiedLoaderRepository2 by seperating the threads attempting class
 loading through it and the single thread active in the UnifiedLoaderRepository2
 by registering a delegate UnifiedClassLoader with the UnifiedLoaderRepository2
 rather than itself. Thus it does not matter how locked up this instance may
 be by the VM since the calling thread has to acquire a singleton lock to
 get into the UnifiedLoaderRepository2, and once in, that thread can never
 interact with any UnifiedClassLoader2, including the instance originating the
 class loading since only the delegate UnifiedClassLoader instances are
 registered with the repository.

 @author <a href="scott.stark@jboss.org">Scott Stark</a>
 @version $Revision: 1.3.4.2 $
*/
public class UnifiedClassLoader2 extends UnifiedClassLoader
   implements UnifiedClassLoader2MBean
{
   // Static --------------------------------------------------------
   private static final Logger log = Logger.getLogger(UnifiedClassLoader2.class);

   // Attributes ----------------------------------------------------

   private UnifiedClassLoader delegate;

   // Constructors --------------------------------------------------
   /**
    * Construct a <tt>UnifiedClassLoader</tt> without registering it to the
    * classloader repository.
    *
    * @param url   the single URL to load classes from.
    */
   public UnifiedClassLoader2(URL url)
   {
      this(url, (URL) null);
   }
   /**
    * Construct a <tt>UnifiedClassLoader</tt> without registering it to the
    * classloader repository.
    *
    * @param url   the single URL to load classes from.
    * @param origURL the possibly null original URL from which url may
    * be a local copy or nested jar.
    */
   public UnifiedClassLoader2(URL url, URL origURL)
   {
      super(url, origURL);
      delegate = new UnifiedClassLoader(url, origURL);
   }

   /**
    * Construct a <tt>UnifiedClassLoader</tt> and registers it to the given
    * repository.
    * @param url The single URL to load classes from.
    * @param origURL the possibly null original URL from which url may
    * be a local copy or nested jar.
    * @param repository the repository this classloader delegates to
    * be a local copy or nested jar.
    */
   public UnifiedClassLoader2(URL url, URL origURL, LoaderRepository repository)
   {
      this(url, origURL);
      
      // set the repository reference
      this.repository = repository;
   }

   // Public --------------------------------------------------------

   public void unregister()
   {
      if (repository != null)
      {
         try
         {
            UnifiedLoaderRepository2.acquire();
            repository.removeClassLoader(delegate);
         }
         finally
         {
            UnifiedLoaderRepository2.release();
         }
      }
   }

   public UnifiedClassLoader getDelegate()
   {
      return delegate;
   }

   /** Get the URL associated with the UCL.
    */
   public URL getURL()
   {
      return delegate.getURL();
   }
   /** Get the original URL associated with the UCL. This may be null.
    */
   public URL getOrigURL()
   {
      return delegate.getOrigURL();
   }

   /**
   * This method simply invokes the super.getURLs() method to access the
   * list of URLs that make up the UnifiedClassLoader classpath.
   */
   public URL[] getClasspath()
   {
      return delegate.getClasspath();
   }

   // URLClassLoader overrides --------------------------------------

   /**
   * We intercept the load class to know exactly the dependencies
   * of the underlying jar.
   *
   * <p>Forwards request to {@link LoaderRepository}.
   */
   public Class loadClass(String name, boolean resolve)
      throws ClassNotFoundException
   {
      // We delegate the repository.
      Class c = null;
      try
      {
         UnifiedLoaderRepository2.acquire();
         c = repository.loadClass(name, resolve, delegate);
      }
      finally
      {
         UnifiedLoaderRepository2.release();
      }
      return c;
   }

   /**
   * Attempts to load the resource from its URL and if not found
   * forwards to the request to {@link LoaderRepository}.
   */
   public URL getResource(String name)
   {
      // We delegate the repository.
      // The repository will decide the strategy to apply
      // when looking for a resource
      URL u = null;
      try
      {
         UnifiedLoaderRepository2.acquire();
         u = repository.getResource(name, delegate);
      }
      finally
      {
         UnifiedLoaderRepository2.release();
      }
      return u;
   }

}
