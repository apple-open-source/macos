/***************************************
*                                     *
*  JBoss: The OpenSource J2EE WebOS   *
*                                     *
*  Distributable under LGPL license.  *
*  See terms of license at gnu.org.   *
*                                     *
***************************************/

package org.jboss.mx.loading;

import EDU.oswego.cs.dl.util.concurrent.ConcurrentReaderHashMap;
import java.net.URL;
import java.net.URLClassLoader;
import java.security.CodeSource;
import java.security.PermissionCollection;
import java.util.Map;
import java.util.Enumeration;
import java.util.Vector;
import java.util.HashSet;
import java.util.HashMap;
import java.io.IOException;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.jboss.logging.Logger;

/**
* A ClassLoader which loads classes from a single URL in conjunction with
* the {@link LoaderRepository}. Notice that this classloader does
* not work independently of the repository. A repository reference
* must be provided via the constructor or the classloader must be explicitly
* registered to the repository before any attempt to load a class.
*
* At this poin this is little more than an abstract class maintained as the
* interface for class loaders as the algorithm of the UnifiedLoaderRepository
* fails with deadlocks, and several other class loading exceptions in multi-
* threaded environments.
*
* @author <a href="marc.fleury@jboss.org">Marc Fleury</a>
* @author <a href="christoph.jung@jboss.org">Christoph G. Jung</a>
* @author <a href="scott.stark@jboss.org">Scott Stark</a>
* @author <a href="juha@jboss.org">Juha Lindfors</a>
* @version <tt>$Revision: 1.9.4.16 $</tt>
*/
public class UnifiedClassLoader
   extends URLClassLoader
   implements UnifiedClassLoaderMBean
{
   // Static --------------------------------------------------------

   private static final Logger log = Logger.getLogger(UnifiedClassLoader.class);
   /** The value returned by {@link getURLs}. */
   private static final URL[] EMPTY_URL_ARRAY = {};

   // Attributes ----------------------------------------------------

   /** Reference to the unified repository. */
   protected LoaderRepository repository = null;

   /** One URL per ClassLoader in our case */
   protected URL url = null;
   /** An optional URL from which url may have been copied. It is used to
    allow the security permissions to be based on a static url namespace. */
   protected URL origURL = null;
   /** Class definitions */
   private Map classes = new ConcurrentReaderHashMap();
   /** The relative order in which this class loader was added to the ULR */
   private int addedOrder;
   private HashSet classBlacklist = new HashSet();
   private HashSet resourceBlackList = new HashSet();
   private HashMap resourceCache = new HashMap();
   private HashMap classCache = new HashMap();

   // Constructors --------------------------------------------------
   /**
    * Construct a <tt>UnifiedClassLoader</tt> without registering it to the
    * classloader repository.
    *
    * @param url   the single URL to load classes from.
    */
   public UnifiedClassLoader(URL url)
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
   public UnifiedClassLoader(URL url, URL origURL)
   {
      this(url, origURL, UnifiedClassLoader.class.getClassLoader());
   }

   /**  Construct a UnifiedClassLoader without registering with the
    * classloader repository.
    *
    * @param url   the single URL to load classes from.
    * @param origURL the possibly null original URL from which url may
    * be a local copy or nested jar.
    * @param parent the parent class loader to use
    */
   public UnifiedClassLoader(URL url, URL origURL, ClassLoader parent)
   {
      super(url == null ? new URL[]{} : new URL[] {url}, parent);

      log.debug("New jmx UCL with url " + url);
      this.url = url;
      this.origURL = origURL;
   }

   /**
    * Construct a <tt>UnifiedClassLoader</tt> and registers it to the given
    * repository.
    *
    * @param   url   The single URL to load classes from.
    * @param   repository the repository this classloader delegates to
    */
   public UnifiedClassLoader(URL url, LoaderRepository repository)
   {
      this(url, null, repository);
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
   public UnifiedClassLoader(URL url, URL origURL, LoaderRepository repository)
   {
      this(url, origURL);

      // set the repository reference
      this.repository = repository;

      // register this loader to the given repository
      repository.addClassLoader(this);
   }

   /**
    * UnifiedClassLoader constructor that can be used to
    * register with a particular Loader Repository identified by ObjectName.
    *
    * @param url an <code>URL</code> value
    * @param server a <code>MBeanServer</code> value
    * @param repositoryName an <code>ObjectName</code> value
    * @exception Exception if an error occurs
    */
   public UnifiedClassLoader(final URL url, final MBeanServer server,
      final ObjectName repositoryName) throws Exception
   {
      this(url, null, server, repositoryName);
   }
   /**
    * UnifiedClassLoader constructor that can be used to
    * register with a particular Loader Repository identified by ObjectName.
    *
    * @param url an <code>URL</code> value
    * @param origURL the possibly null original URL from which url may
    * be a local copy or nested jar.
    * @param server a <code>MBeanServer</code> value
    * @param repositoryName an <code>ObjectName</code> value
    * @exception Exception if an error occurs
    */
   public UnifiedClassLoader(final URL url, final URL origURL,
      final MBeanServer server, final ObjectName repositoryName) throws Exception
   {
      this(url, origURL);
      this.repository = (LoaderRepository)server.invoke(repositoryName,
                    "registerClassLoader",
                    new Object[] {this},
                    new String[] {getClass().getName()});
   }


   /**
    * Constructs a <tt>UnifiedClassLoader</tt> with given class definition.
    *
    * @param   names class name
    * @param   codes class definition
    */
   public UnifiedClassLoader(String name, byte[] code)
   {
      // FIXME: UCL cloader or ctx cl as parent??
      super(new URL[] {}, UnifiedClassLoader.class.getClassLoader());
      addClass(name, code);
   }


   // Public --------------------------------------------------------

   public int getAddedOrder()
   {
      return addedOrder;
   }
   public void setAddedOrder(int addedOrder)
   {
      this.addedOrder = addedOrder;
   }

   /** Obtain the ObjectName under which the UCL can be registered with the
    JMX server. This creates a name of the form "jmx.loading:UCL=hashCode"
    since we don't currently care that UCL be easily queriable.
    */
   public ObjectName getObjectName() throws MalformedObjectNameException
   {
      String name = "jmx.loading:UCL="+Integer.toHexString(super.hashCode());
      return new ObjectName(name);
   }

   public void unregister()
   {
      if (repository != null)
      {
         repository.removeClassLoader(this);
      }
      this.classes.clear();
      this.origURL = null;
      this.url = null;
      this.repository = null;
   }

   /** Append the given url to the URLs used for class and resource loading
    * @param url the URL to load from
    */
   public void addURL(URL url)
   {
      if( url == null )
         throw new IllegalArgumentException("url cannot be null");

      if( repository.addClassLoaderURL(this, url) == true )
      {
         log.debug("Added url: "+url+", to ucl: "+this);
         super.addURL(url);
      }
      else if( log.isTraceEnabled() )
      {
         log.trace("Ignoring duplicate url: "+url+", for ucl: "+this);
      }
   }

   public void addClass(String name, byte[] code)
   {
      classes.put(name, code);
   }

   public LoaderRepository getLoaderRepository()
   {
      return repository;
   }

   public void setRepository(LoaderRepository repository)
   {
      this.repository = repository;
   }

   /** Called to attempt to load a class from the set of URLs associated with
    * the UCL.
    */
   public Class loadClassLocally(String name, boolean resolve)
      throws ClassNotFoundException
   {
      boolean trace = log.isTraceEnabled();
      if( trace )
         log.trace("loadClassLocally, name="+name);
      Class clazz = (Class) classCache.get(name);
      if (clazz != null)
      {
         if( trace )
            log.trace("Found cached class: "+clazz);
         return clazz;
      }

      if (classBlacklist.contains(name))
      {
         if( trace )
            log.trace("Class in blacklist, name="+name);
         throw new ClassNotFoundException("Class Not Found(blacklist): " + name);
      }

      try
      {
         return clazz = super.loadClass(name, resolve);
      }
      catch (ClassNotFoundException cnfe)
      {
         classBlacklist.add(name);
         throw cnfe;
      }
   }

   /**
   * Provides the same functionality as {@link java.net.URLClassLoader#getResource}.
   */
   public URL getResourceLocally(String name)
   {
      URL resURL = (URL)resourceCache.get(name);
      if (resURL != null) return resURL;
      if (resourceBlackList.contains(name)) return null;
      resURL = super.getResource(name);
      if( log.isTraceEnabled() == true )
         log.trace("getResourceLocally("+this+"), name="+name+", resURL:"+resURL);
      if (resURL == null) resourceBlackList.add(name);
      else resourceCache.put(name, resURL);
      return resURL;
   }

   /** Get the URL associated with the UCL.
    */
   public URL getURL()
   {
      return url;
   }
   /** Get the original URL associated with the UCL. This may be null.
    */
   public URL getOrigURL()
   {
      return origURL;
   }

   /**
   * This method simply invokes the super.getURLs() method to access the
   * list of URLs that make up the UnifiedClassLoader classpath.
   */
   public URL[] getClasspath()
   {
      return super.getURLs();
   }

   // URLClassLoader overrides --------------------------------------

   /** The only caller of this method should be the VM initiated
   * loadClassInternal() method. This method attempts to acquire the
   * UnifiedLoaderRepository2 lock and then asks the repository to
   * load the class.
   *
   * <p>Forwards request to {@link LoaderRepository}.
   */
   public Class loadClass(String name, boolean resolve)
      throws ClassNotFoundException
   {
      Class c = repository.loadClass(name, resolve, this);
      return c;
   }

   /**
   * Attempts to load the resource from its URL and if not found
   * forwards to the request to {@link LoaderRepository}.
   */
   public URL getResource(String name)
   {
      URL res = repository.getResource(name, this);
      return res;
   }

   /** Find all resource URLs for the given name. This overrides the
    * URLClassLoader version to look for resources in the repository.
    *
    * @param name the name of the resource
    * @return Enumeration<URL>
    * @throws java.io.IOException
    */
   public Enumeration findResources(String name) throws IOException
   {
      Vector resURLs = new Vector();
      repository.getResources(name, this, resURLs);
      return resURLs.elements();
   }

   /**
   * Provides the same functionality as {@link java.net.URLClassLoader#findResources}.
   */
   public Enumeration findResourcesLocally(String name) throws IOException
   {
      Enumeration resURLs = super.findResources(name);
      return resURLs;
   }

   /** This is here to document that this must delegate to the
   super implementation to perform identity based hashing. Using
   URL based hashing caused conflicts with the Class.forName(String,
   boolean, ClassLoader).
   */
   public final int hashCode()
   {
      int hash = super.hashCode();
      return hash;
   }

   /** This is here to document that this must delegate to the
   super implementation to perform identity based equality. Using
   URL based equality caused conflicts with the Class.forName(String,
   boolean, ClassLoader).
   */
   public final boolean equals(Object other)
   {
      boolean equals = super.equals(other);
      return equals;
   }
   /**
   * Return all library URLs associated with this UnifiedClassLoader
   *
   * <p>Do not remove this method without running the WebIntegrationTestSuite
   */
   public URL[] getAllURLs()
   {
      return repository.getURLs();
   }

   /**
   * Return an empty URL array to force the RMI marshalling subsystem to
   * use the <tt>java.server.codebase</tt> property as the annotated codebase.
   *
   * <p>Do not remove this method without discussing it on the dev list.
   *
   * @return Empty URL[]
   */
   public URL[] getURLs()
   {
      return EMPTY_URL_ARRAY;
   }

   public Package getPackage(String name)
   {
      return super.getPackage(name);
   }
   public Package[] getPackages()
   {
      return super.getPackages();
   }

   /**
   * Retruns a string representaion of this UCL.
   */
   public String toString()
   {
      return super.toString() + "{ url=" + getURL() + " }";
   }

   /**
   *
   */
   protected Class findClass(String name) throws ClassNotFoundException
   {
      Object o = classes.get(name);
      Class clazz = null;
      if (o != null)
      {
         byte[] code = (byte[])o;
         classes.remove(name);
         clazz = defineClass(name, code, 0, code.length);
      }
      else
      {
         clazz = super.findClass(name);
      }
      return clazz;
   }

   /** Override the permissions accessor to build a code source
    based on the original URL if one exists. This allows the
    security policy to be defined in terms of the static URL
    namespace rather than the local copy or nested URL.
    @param cs the location and signatures of the codebase.
    */
   protected PermissionCollection getPermissions(CodeSource cs)
   {
      CodeSource permCS = cs;
      if( origURL != null )
      {
         permCS = new CodeSource(origURL, cs.getCertificates());
      }
      PermissionCollection perms = super.getPermissions(permCS);
      if( log.isTraceEnabled() )
         log.trace("getPermissions, url="+url+", origURL="+origURL+" -> "+perms);
      return perms;
   }

}
