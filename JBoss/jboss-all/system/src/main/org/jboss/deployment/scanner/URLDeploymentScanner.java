/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.deployment.scanner;

import java.io.File;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLConnection;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;
import java.util.StringTokenizer;
import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.jboss.deployment.IncompleteDeploymentException;
import org.jboss.net.protocol.URLLister;
import org.jboss.net.protocol.URLListerFactory;
import org.jboss.system.server.ServerConfigLocator;
import org.jboss.system.server.ServerConfig;
import org.jboss.util.NullArgumentException;


/**
 * A URL-based deployment scanner.  Supports local directory
 * scanning for file-based urls.
 *
 * @jmx:mbean extends="org.jboss.deployment.scanner.DeploymentScannerMBean"
 *
 * @version <tt>$Revision: 1.20.2.9 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class URLDeploymentScanner
   extends AbstractDeploymentScanner
   implements DeploymentScanner, URLDeploymentScannerMBean
{
   /** The list of URLs to scan. */
   protected List urlList = Collections.synchronizedList(new ArrayList());

   /** A set of scanned urls which have been deployed. */
   protected Set deployedSet = Collections.synchronizedSet(new HashSet());

   /** The server's home directory, for relative paths. */
   protected File serverHome;

   protected URL serverHomeURL;

   /** A sorter urls from a scaned directory to allow for coarse dependency
    ordering based on file type
    */
   protected Comparator sorter;

   /** Allow a filter for scanned directories */
   protected URLLister.URLFilter filter;
   protected IncompleteDeploymentException lastIncompleteDeploymentException;
   
   // indicates if we should directly search for files inside directories 
   // whose names containing no dots
   //
   protected boolean doRecursiveSearch = true;

   
   /**
    * @jmx:managed-attribute
    */
   public void setRecursiveSearch (boolean recurse)
   {
      doRecursiveSearch = recurse;
   }
   
   /**
    * @jmx:managed-attribute
    */
   public boolean getRecursiveSearch ()
   {
      return doRecursiveSearch;
   }
   
   /**
    * @jmx:managed-attribute
    */
   public void setURLList(final List list)
   {
      if (list == null)
         throw new NullArgumentException("list");

      boolean debug = log.isDebugEnabled();

      // start out with a fresh list
      urlList.clear();

      Iterator iter = list.iterator();
      while (iter.hasNext())
      {
         URL url = (URL)iter.next();
         if (url == null)
            throw new NullArgumentException("list element");

         addURL(url);
      }

      if (debug)
      {
         log.debug("URL list: " + urlList);
      }
   }

   /**
    * @jmx:managed-attribute
    *
    * @param classname    The name of a Comparator class.
    */
   public void setURLComparator(String classname)
      throws ClassNotFoundException, IllegalAccessException,
      InstantiationException
   {
      sorter = (Comparator)Thread.currentThread().getContextClassLoader().loadClass(classname).newInstance();
   }

   /**
    * @jmx:managed-attribute
    */
   public String getURLComparator()
   {
      if (sorter == null)
         return null;
      return sorter.getClass().getName();
   }

   /**
    * @jmx:managed-attribute
    *
    * @param classname    The name of a FileFilter class.
    */
   public void setFilter(String classname)
   throws ClassNotFoundException, IllegalAccessException, InstantiationException
   {
      Class filterClass = Thread.currentThread().getContextClassLoader().loadClass(classname);
      filter = (URLLister.URLFilter) filterClass.newInstance();
   }

   /**
    * @jmx:managed-attribute
    */
   public String getFilter()
   {
      if (filter == null)
         return null;
      return filter.getClass().getName();
   }

   /**
    * @jmx:managed-attribute
    */
   public List getURLList()
   {
      // too bad, List isn't a cloneable
      return new ArrayList(urlList);
   }

   /**
    * @jmx:managed-operation
    */
   public void addURL(final URL url)
   {
      if (url == null)
         throw new NullArgumentException("url");

      urlList.add(url);
      if (log.isDebugEnabled())
      {
         log.debug("Added url: " + url);
      }
   }

   /**
    * @jmx:managed-operation
    */
   public void removeURL(final URL url)
   {
      if (url == null)
         throw new NullArgumentException("url");

      boolean success = urlList.remove(url);
      if (success && log.isDebugEnabled())
      {
         log.debug("Removed url: " + url);
      }
   }

   /**
    * @jmx:managed-operation
    */
   public boolean hasURL(final URL url)
   {
      if (url == null)
         throw new NullArgumentException("url");

      return urlList.contains(url);
   }


   /////////////////////////////////////////////////////////////////////////
   //                  Management/Configuration Helpers                   //
   /////////////////////////////////////////////////////////////////////////

   /**
    * @jmx:managed-attribute
    */
   public void setURLs(final String listspec) throws MalformedURLException
   {
      if (listspec == null)
         throw new NullArgumentException("listspec");

      boolean debug = log.isDebugEnabled();

      List list = new LinkedList();

      StringTokenizer stok = new StringTokenizer(listspec, ",");
      while (stok.hasMoreTokens())
      {
         String urlspec = stok.nextToken().trim();

         if (debug)
         {
            log.debug("Adding URL from spec: " + urlspec);
         }

         URL url = makeURL(urlspec);
         if (debug)
         {
            log.debug("URL: " + url);
         }
         list.add(url);
      }

      setURLList(list);
   }

   /**
    * A helper to make a URL from a full url, or a filespec.
    */
   protected URL makeURL(String urlspec) throws MalformedURLException
   {
      // First replace URL with appropriate properties
      //
      urlspec = org.jboss.util.Strings.replaceProperties (urlspec);
      return new URL(serverHomeURL, urlspec);
   }

   /**
    * @jmx:managed-operation
    */
   public void addURL(final String urlspec) throws MalformedURLException
   {
      addURL(makeURL(urlspec));
   }

   /**
    * @jmx:managed-operation
    */
   public void removeURL(final String urlspec) throws MalformedURLException
   {
      removeURL(makeURL(urlspec));
   }

   /**
    * @jmx:managed-operation
    */
   public boolean hasURL(final String urlspec) throws MalformedURLException
   {
      return hasURL(makeURL(urlspec));
   }

   /**
    * A helper to deploy the given URL with the deployer.
    */
   protected void deploy(final DeployedURL du)
   {
      // If the deployer is null simply ignore the request
      if( deployer == null )
         return;

      if (log.isTraceEnabled())
      {
         log.trace("Deploying: " + du);
      }
      try
      {
         deployer.deploy(du.url);
      }
      catch (IncompleteDeploymentException e)
      {
         lastIncompleteDeploymentException = e;
      }
      catch (Exception e)
      {
         log.debug("Failed to deploy: " + du, e);
      } // end of try-catch

      du.deployed();

      if (!deployedSet.contains(du))
      {
         deployedSet.add(du);
      }
   }

   /**
    * A helper to undeploy the given URL from the deployer.
    */
   protected void undeploy(final DeployedURL du)
   {
      try
      {
         if (log.isTraceEnabled())
         {
            log.trace("Undeploying: " + du);
         }
         deployer.undeploy(du.url);
         deployedSet.remove(du);
      }
      catch (Exception e)
      {
         log.error("Failed to undeploy: " + du, e);
      }
   }

   /**
    * Checks if the url is in the deployed set.
    */
   protected boolean isDeployed(final URL url)
   {
      DeployedURL du = new DeployedURL(url);
      return deployedSet.contains(du);
   }

   public synchronized void scan() throws Exception
   {
      lastIncompleteDeploymentException = null;
      if (urlList == null)
         throw new IllegalStateException("not initialized");

      boolean trace = log.isTraceEnabled();
      URLListerFactory factory = new URLListerFactory();
      List urlsToDeploy = new LinkedList();

      // Scan for deployments
      if (trace)
      {
         log.trace("Scanning for new deployments");
      }
      synchronized (urlList)
      {
         for (Iterator i = urlList.iterator(); i.hasNext();)
         {
            URL url = (URL) i.next();
            if (url.toString().endsWith("/"))
            {
               // treat URL as a collection
               URLLister lister = factory.createURLLister(url);
               urlsToDeploy.addAll(lister.listMembers(url, filter, doRecursiveSearch));
            }
            else
            {
               // treat URL as a deployable unit
               urlsToDeploy.add(url);
            }
         }
      }

      if (trace)
      {
         log.trace("Updating existing deployments");
      }
      LinkedList urlsToRemove = new LinkedList();
      LinkedList urlsToCheckForUpdate = new LinkedList();
      synchronized (deployedSet)
      {
         // remove previously deployed URLs no longer needed
         for (Iterator i = deployedSet.iterator(); i.hasNext();)
         {
            DeployedURL deployedURL = (DeployedURL) i.next();
            if (urlsToDeploy.contains(deployedURL.url))
            {
               urlsToCheckForUpdate.add(deployedURL);
            }
            else
            {
               urlsToRemove.add(deployedURL);
            }
         }
      }

      // ********
      // Undeploy
      // ********

      for (Iterator i = urlsToRemove.iterator(); i.hasNext();)
      {
         DeployedURL deployedURL = (DeployedURL) i.next();
         if (trace)
         {
            log.trace("Removing " + deployedURL.url);
         }
         undeploy(deployedURL);
      }

      // ********
      // Redeploy
      // ********

      // compute the DeployedURL list to update
      ArrayList urlsToUpdate = new ArrayList(urlsToCheckForUpdate.size());
      for (Iterator i = urlsToCheckForUpdate.iterator(); i.hasNext();)
      {
         DeployedURL deployedURL = (DeployedURL) i.next();
         if (deployedURL.isModified())
         {
            if (trace)
            {
               log.trace("Re-deploying " + deployedURL.url);
            }
            urlsToUpdate.add(deployedURL);
         }
      }

      // sort to update list
      Collections.sort(urlsToUpdate, new Comparator()
      {
         public int compare(Object o1, Object o2)
         {
            return sorter.compare(((DeployedURL) o1).url, ((DeployedURL) o2).url);
         }
      });

      // Undeploy in order
      for (int i = urlsToUpdate.size() - 1; i >= 0;i--)
      {
         undeploy((DeployedURL) urlsToUpdate.get(i));
      }

      // Deploy in order
      for (int i = 0; i < urlsToUpdate.size();i++)
      {
         deploy((DeployedURL) urlsToUpdate.get(i));
      }

      // ******
      // Deploy
      // ******

      Collections.sort(urlsToDeploy, sorter);
      for (Iterator i = urlsToDeploy.iterator(); i.hasNext();)
      {
         URL url = (URL) i.next();
         DeployedURL deployedURL = new DeployedURL(url);
         if (deployedSet.contains(deployedURL) == false)
         {
            if (trace)
            {
               log.trace("Deploying " + deployedURL.url);
            }
            deploy(deployedURL);
         }
      }

      // Validate that there are still incomplete deployments
      if (lastIncompleteDeploymentException != null)
      {
         try
         {
            Object[] args = {};
            String[] sig = {};
            getServer().invoke(getDeployer(),
                               "checkIncompleteDeployments", args, sig);
         }
         catch (Exception e)
         {
            log.error(e);
         }
      }
   }


   /////////////////////////////////////////////////////////////////////////
   //                     Service/ServiceMBeanSupport                     //
   /////////////////////////////////////////////////////////////////////////

   public ObjectName preRegister(MBeanServer server, ObjectName name)
      throws Exception
   {
      // get server's home for relative paths, need this for setting
      // attribute final values, so we need to do it here
      ServerConfig serverConfig = ServerConfigLocator.locate();
      serverHome = serverConfig.getServerHomeDir();
      serverHomeURL = serverConfig.getServerHomeURL();

      return super.preRegister(server, name);
   }

   /////////////////////////////////////////////////////////////////////////
   //                           DeployedURL                               //
   /////////////////////////////////////////////////////////////////////////

   /**
    * A container and help class for a deployed URL.
    * should be static at this point, with the explicit scanner ref, but I'm (David) lazy.
    */
   protected class DeployedURL
   {
      public URL url;
      public URL watchUrl;
      public long deployedLastModified;

      public DeployedURL(final URL url)
      {
         this.url = url;
      }

      public void deployed()
      {
         deployedLastModified = getLastModified();
      }
      public boolean isFile()
      {
         return url.getProtocol().equals("file");
      }

      public File getFile()
      {
         return new File(url.getFile());
      }

      public boolean isRemoved()
      {
         if (isFile())
         {
            File file = getFile();
            return !file.exists();
         }
         return false;
      }

      public long getLastModified()
      {
         if (watchUrl == null)
         {
            //
            // jason: getWatchUrl() is not part of Deployer interface.. wtf is this?
            //
            try
            {
               Object o = getServer().invoke(getDeployer(), "getWatchUrl",
               new Object[] { url },
               new String[] { URL.class.getName() });
               watchUrl = o == null ? url : (URL)o;
               getLog().debug("Watch URL for: " + url + " -> " + watchUrl);
            }
            catch (Exception e)
            {
               watchUrl = url;
               getLog().debug("Unable to obtain watchUrl from deployer. Use url: " + url, e);
            }
         }

         try
         {
            URLConnection connection;
            if (watchUrl != null)
            {
               connection = watchUrl.openConnection();
            } else
            {
               connection = url.openConnection();
            }
            // no need to do special checks for files...
            // org.jboss.net.protocol.file.FileURLConnection correctly
            // implements the getLastModified method.
            long lastModified = connection.getLastModified();

            return lastModified;
         }
         catch (java.io.IOException e)
         {
            log.warn("Failed to check modfication of deployed url: " + url, e);
         }

         return -1;
      }

      public boolean isModified()
      {
         long lastModified = getLastModified();
         if (lastModified == -1) {
            // ignore errors fetching the timestamp - see bug 598335
            return false;
         }
         return deployedLastModified != lastModified;
      }

      public int hashCode()
      {
         return url.hashCode();
      }

      public boolean equals(final Object other)
      {
         if (other instanceof DeployedURL)
         {
            return ((DeployedURL)other).url.equals(this.url);
         }
         return false;
      }

      public String toString()
      {
         return super.toString() +
         "{ url=" + url +
         ", deployedLastModified=" + deployedLastModified +
         " }";
      }
   }
}
