package org.jboss.web.tomcat.tc5;

import java.beans.PropertyChangeListener;
import java.net.URL;
import java.net.MalformedURLException;
import java.net.URLClassLoader;
import java.io.File;
import java.io.IOException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Enumeration;
import javax.servlet.ServletContext;

import org.apache.catalina.Lifecycle;
import org.apache.catalina.Loader;
import org.apache.catalina.LifecycleListener;
import org.apache.catalina.LifecycleException;
import org.apache.catalina.Container;
import org.apache.catalina.DefaultContext;
import org.apache.catalina.Context;
import org.apache.catalina.Globals;
import org.apache.naming.resources.DirContextURLStreamHandler;

import org.jboss.mx.loading.UnifiedClassLoader;

/** Initial version of a JBoss implementation of the Tomcat Loader.
 * 
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class WebCtxLoader
   implements Lifecycle, Loader
{
   /** The ClassLoader used to scope the ENC */
   protected ClassLoader encLoader;
   /** The ClassLoader returned from getClassLoader */
   protected ENCLoader ctxLoader;
   /** The war UCL used to load the war classes */
   protected UnifiedClassLoader delegate;
   protected Container webContainer;
   protected URL warURL;

   /**
    * The set of repositories associated with this class loader.
    */
   private ArrayList repositories = new ArrayList();

   /** Create a WebCtxLoader given the ENC scoping class loader.
    * @param encLoader
    */
   WebCtxLoader(ClassLoader encLoader)
   {
      this.encLoader = encLoader;
      this.ctxLoader = new ENCLoader(encLoader);
      ClassLoader parent = encLoader;
      while ((parent instanceof UnifiedClassLoader) == false && parent != null)
         parent = parent.getParent();
      this.delegate = (UnifiedClassLoader) parent;
   }

   public void setWarURL(URL warURL) throws MalformedURLException
   {
      this.warURL = warURL;
      String path = warURL.getFile();
      File classesDir = new File(path, "WEB-INF/classes");
      if (classesDir.exists())
         delegate.addURL(classesDir.toURL());
      File libDir = new File(path, "WEB-INF/lib");
      if (libDir.exists())
      {
         File[] jars = libDir.listFiles();
         int length = jars != null ? jars.length : 0;
         for (int j = 0; j < length; j++)
            delegate.addURL(jars[j].toURL());
      }
   }

   public void addLifecycleListener(LifecycleListener listener)
   {
   }

   public LifecycleListener[] findLifecycleListeners()
   {
      return new LifecycleListener[0];
   }

   public void removeLifecycleListener(LifecycleListener listener)
   {
   }

   public void start() throws LifecycleException
   {
       setClassPath();
      ServletContext servletContext = ((Context) webContainer).getServletContext();
      if (servletContext == null)
         return;
   }

   public void stop() throws LifecycleException
   {
      // Remove the ctxLoader mapping kept by the DirContextURLStreamHandler 
      DirContextURLStreamHandler.unbind(ctxLoader);
      org.apache.commons.logging.LogFactory.release(ctxLoader);
      this.encLoader = null;
      this.ctxLoader = null;
      this.delegate = null;
      this.repositories.clear();
      this.warURL = null;
      this.webContainer = null;
   }

   /** We must pass the wrapped encLoader as tomcat needs to see a unique
    * class loader that is distinct from the thread context class loader seen
    * to be in effect when the web app is started. This is due to how it
    * binds contexts using the DirContextURLStreamHandler class.
    * 
    * @see org.apache.naming.resources.DirContextURLStreamHandler
    * @return The ENC scoping class loader
    */
   public ClassLoader getClassLoader()
   {
      return ctxLoader;
   }

   public Container getContainer()
   {
      return webContainer;
   }

   public void setContainer(Container container)
   {
      webContainer = container;

   }

   public DefaultContext getDefaultContext()
   {
      return null;
   }

   public void setDefaultContext(DefaultContext defaultContext)
   {
   }

   public boolean getDelegate()
   {
      return false;
   }

   public void setDelegate(boolean delegate)
   {
   }

   public String getInfo()
   {
      return null;
   }

   public boolean getReloadable()
   {
      return false;
   }

   public void setReloadable(boolean reloadable)
   {
   }

   public void addPropertyChangeListener(PropertyChangeListener listener)
   {
   }

   public void addRepository(String repository)
   {
      if( repositories.contains(repository) == true )
         return;
      repositories.add(repository);
      setClassPath();
   }

   public String[] findRepositories()
   {
      String[] tmp = new String[repositories.size()];
      repositories.toArray(tmp);
      return tmp;
   }

   public boolean modified()
   {
      return false;
   }

   public void removePropertyChangeListener(PropertyChangeListener listener)
   {
   }

   /**
    * Set the appropriate context attribute for our class path.  This
    * is required only because Jasper depends on it.
    */
   private void setClassPath()
   {
      // Validate our current state information
      if (!(webContainer instanceof Context))
         return;
      ServletContext servletContext = ((Context) webContainer).getServletContext();
      if (servletContext == null)
         return;

      try {
          Method method =
              webContainer.getClass().getMethod("getCompilerClasspath", null);
          Object baseClasspath = method.invoke(webContainer, null);
          if (baseClasspath != null) {
              servletContext.setAttribute(Globals.CLASS_PATH_ATTR,
                                          baseClasspath.toString());
              return;
          }
      } catch (Exception e) {
          // Ignore
          e.printStackTrace();
      }

      StringBuffer classpath = new StringBuffer();

      // Assemble the class path information from our repositories
      for (int i = 0; i < repositories.size(); i++)
      {
         String repository = repositories.get(i).toString();
         if (repository.startsWith("file://"))
            repository = repository.substring(7);
         else if (repository.startsWith("file:"))
            repository = repository.substring(5);
         else if (repository.startsWith("jndi:"))
            repository = servletContext.getRealPath(repository.substring(5));
         else
            continue;
         if (repository == null)
            continue;
         if (i > 0)
            classpath.append(File.pathSeparator);
         classpath.append(repository);
      }

      // Store the assembled class path as a servlet context attribute
      servletContext.setAttribute(Globals.CLASS_PATH_ATTR,
         classpath.toString());

   }

   /** A trival extension of URLClassLoader that uses an empty URL[] as its
    * classpath so that all work is delegated to its parent.
    */ 
   static class ENCLoader extends URLClassLoader
   {
      ENCLoader(ClassLoader parent)
      {
         super(new URL[0], parent);
      }
   }
}
