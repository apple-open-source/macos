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
import java.io.FileFilter;
import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLConnection;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Iterator;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import org.w3c.dom.Element;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

import org.jboss.deployment.Deployer;
import org.jboss.deployment.DeploymentException;
import org.jboss.system.server.ServerConfigLocator;

/**
 * This class is similar to the URLDeploymentScanner (
 * @see org.jboss.deployment.scanner.URLDeploymentScanner).  It is designed
 * to deploy direct URLs and to scan directories.  The distinction between
 * the two is that this class forces the user to specify which entries are
 * directories to scan, and which are urls to deploy.
 *
 * @jmx:mbean extends="org.jboss.deployment.scanner.DeploymentScannerMBean"
 *
 * @version
 * @author  <a href="mailto:lsanders@speakeasy.net">Larry Sanderson</a>
 */
public class URLDirectoryScanner extends AbstractDeploymentScanner
   implements DeploymentScanner, URLDirectoryScannerMBean
{
   
   /** This is the default file filter for directory scanning */
   private FileFilter defaultFilter;
   
   /** This is the default Comparator for directory deployment ordering */
   private Comparator defaultComparator;
   
   /** This is the list of scanner objects */
   private ArrayList scanners = new ArrayList();
   
   /** This is a URL to scanner map. */
   private HashMap urlScannerMap = new HashMap();
   
   /** this is used for resolution of reletive file paths */
   private File serverHome;
   
   /**
    * @jmx:managed-attribute
    */
   public void addScanURL(URL url)
   {
      Scanner scanner = new Scanner(url);
      addScanner(scanner);
   }
   
   /**
    * @jmx:managed-attribute
    */
   public void addScanURL(String url) throws MalformedURLException
   {
      addScanURL(toUrl(url));
   }
   
   /**
    * @jmx:managed-attribute
    */
   public void addScanDir(URL url, Comparator comp, FileFilter filter)
   {
      Scanner scanner = new DirScanner(url, comp, filter);
      addScanner(scanner);
   }
   
   /**
    * @jmx:managed-attribute
    */
   public void addScanDir(String urlSpec, String compClassName, String filterClassName)
      throws MalformedURLException
   {
      
      URL url = toUrl(urlSpec);
      // create a new comparator
      Comparator comp = null;
      if (compClassName != null)
      {
         try
         {
            Class compClass = Thread.currentThread().getContextClassLoader().loadClass("compClassName");
            comp = (Comparator)compClass.newInstance();
         } catch (Exception e)
         {
            log.warn("Unable to create instance of Comparator.  Ignoring.", e);
         }
      }
      // create a new filter
      FileFilter filter = null;
      if (filterClassName != null)
      {
         try
         {
            Class filterClass = Thread.currentThread().getContextClassLoader().loadClass(filterClassName);
            filter = (FileFilter)filterClass.newInstance();
         } catch (Exception e)
         {
            log.warn("Unable to create instance of Filter.  Ignoring.", e);
         }
      }
      
      addScanDir(url, comp, filter);
   }
   
   private void addScanner(Scanner scanner)
   {
      synchronized (scanners)
      {
         if (isScanEnabled())
         {
            // Scan is enabled, so apply changes to a local copy
            // this enables the scan to occur while things are added
            ArrayList localScanners = new ArrayList(scanners);
            HashMap localMap = new HashMap(urlScannerMap);
            
            localScanners.add(scanner);
            localMap.put(scanner.url, scanner);
            
            scanners = localScanners;
            urlScannerMap = localMap;
         } else
         {
            // no need for precautions... just add
            scanners.add(scanner);
            urlScannerMap.put(scanner.url, scanner);
         }
      }
   }
   
   /**
    * @jmx:managed-attribute
    */
   public void removeScanURL(URL url)
   {
      synchronized (scanners)
      {
         if (isScanEnabled())
         {
            // Scan is enabled, so apply changes to a local copy
            // this enables the scan to occur while things are added
            ArrayList localScanners = new ArrayList(scanners);
            HashMap localMap = new HashMap(urlScannerMap);
            
            Scanner scanner = (Scanner)localMap.remove(url);
            if (scanner != null)
            {
               localScanners.remove(scanner);
            }
            
            scanners = localScanners;
            urlScannerMap = localMap;
         } else
         {
            // no need for precautions... just remove
            Scanner scanner = (Scanner)urlScannerMap.remove(url);
            if (scanner != null)
            {
               scanners.remove(scanner);
            }
         }
      }
   }
   
   /**
    * @jmx:managed-attribute
    */
   public void setURLs(Element elem)
   {
      // create local versions of these collections
      ArrayList localScanners = new ArrayList();
      HashMap localMap = new HashMap();
      
      NodeList list = elem.getChildNodes();
      synchronized (scanners)
      {
         // clear lists
         scanners.clear();
         urlScannerMap.clear();
         
         // populate from xml....
         for (int i = 0; i < list.getLength(); i++)
         {
            Node node = list.item(i);
            if (node.getNodeType() == Node.ELEMENT_NODE) {
               NamedNodeMap nodeMap = node.getAttributes();
               String name = getNodeValue(nodeMap.getNamedItem("name"));
               if (name == null)
               {
                  log.warn("No name specified in URLDirectoryScanner config: " +
                  node + ".  Ignoring");
                  continue;
               }

               try
               {
                  if (node.getNodeName().equals("dir"))
                  {
                     // get the filter and comparator
                     String filter = getNodeValue(nodeMap.getNamedItem("filter"));
                     String comp = getNodeValue(nodeMap.getNamedItem("comparator"));

                     addScanDir(name, comp, filter);
                  } else if (node.getNodeName().equals("url"))
                  {
                     addScanURL(name);
                  }
               } catch (MalformedURLException e)
               {
                  log.warn("Invalid url in DeploymentScanner.  Ignoring.", e);
               }
            }
         }
      }
   }
   
   /**
    * Extract a string from a node.  Trim the value (down to null for empties).
    */
   private String getNodeValue(Node node)
   {
      if (node == null)
      {
         return null;
      }
      String val = node.getNodeValue();
      if (val == null)
      {
         return null;
      }
      if ((val = val.trim()).length() == 0)
      {
         return null;
      }
      return val;
   }
   
   /**
    * Convert a string to a node.  I really just copied the code from
    * Jason Dillon's URLDeploymentScanner.  Thanks Jason!
    */
   private URL toUrl(String value) throws MalformedURLException
   {
      try
      {
         return new URL(value);
      } catch (MalformedURLException e)
      {
         File file = new File(value);
         if (!file.isAbsolute())
         {
            file = new File(serverHome, value);
         }
         
         try
         {
            file = file.getCanonicalFile();
         } catch (IOException ioe)
         {
            throw new MalformedURLException("Can not obtain file: " + ioe);
         }
         
         return file.toURL();
      }
   }
   
   /**
    * @jmx:managed-attribute
    */
   public void setURLComparator(String comparatorClassName)
   {
      log.debug("Setting Comparator: " + comparatorClassName);
      try
      {
         defaultComparator = (Comparator)Thread.currentThread().getContextClassLoader().loadClass(comparatorClassName).newInstance();
      } catch (Exception e)
      {
         log.warn("Unable to create URLComparator.", e);
      }
   }
   
   /**
    * @jmx:managed-attribute
    */
   public String getURLComparator()
   {
      if (defaultComparator == null)
      {
         return null;
      }
      return defaultComparator.getClass().getName();
   }
   
   /**
    * @jmx:managed-attribute
    */
   public void setFilter(String filterClassName)
   {
      log.debug("Setting Filter: " + filterClassName);
      try
      {
         defaultFilter = (FileFilter)Thread.currentThread().getContextClassLoader().loadClass(filterClassName).newInstance();
      } catch (Exception e)
      {
         log.warn("Unable to create URLComparator.", e);
      }
   }
   
   /**
    * @jmx:managed-attribute
    */
   public String getFilter()
   {
      if (defaultFilter == null)
      {
         return null;
      }
      return defaultFilter.getClass().getName();
   }
   
   /**
    * This is a workaround for a bug in Sun's JVM 1.3 on windows (any
    * others??).  Inner classes can not directly access protected members
    * from the outer-class's super class.
    */
   Deployer getDeployerObj()
   {
      return deployer;
   }
   
   /**
    * This class scans a single url for modifications.  It supports
    * missing url's, and will deploy them when they appear.
    */
   private class Scanner
   {
      /** the original url to scan */
      protected URL url;
      
      /** the url to watch for modification */
      private URL watchUrl;
      
      /** this holds the lastModified time of watchUrl */
      private long lastModified;
      
      /** this is a flag to indicate if this url is deployed */
      private boolean deployed;
      
      /**
       * Construct with the url to deploy / watch
       */
      public Scanner(URL url)
      {
         this.url = url;
      }
      
      /**
       * Check the url for modification, and deploy / redeploy / undeploy
       * appropriately.
       */
      public void scan()
      {
         if (getLog().isTraceEnabled()) {
            getLog().trace("Scanning url: " + url);
         }
         // check time stamps
         if (lastModified != getLastModified())
         {
            if (exists())
            {
               // url needs deploy / redeploy
               try
               {
                  getLog().debug("Deploying Modified (or new) url: " + url);
                  deploy();
               } catch (DeploymentException e)
               {
                  getLog().error("Failed to (re)deploy url: " + url, e);
               }
            } else
            {
               // url does not exist... undeploy
               try
               {
                  getLog().debug("Undeploying url: " + url);
                  undeploy();
               } catch (DeploymentException e)
               {
                  getLog().error("Failed to undeploy url: " + url, e);
               }
            }
         }
      }
      
      /**
       * Return true if the url exists, false otherwise.
       */
      private boolean exists()
      {
         try
         {
            if (url.getProtocol().equals("file")) {
               File file = new File(url.getPath());
               return file.exists();
            } else {
               url.openStream().close();
               return true;
            }
         } catch (IOException e)
         {
            return false;
         }
      }
      /**
       * return the modification date of watchUrl
       */
      private long getLastModified()
      {
         try
         {
            URL lmUrl = watchUrl == null ? url : watchUrl;
            return lmUrl.openConnection().getLastModified();
         } catch (IOException e)
         {
            return 0L;
         }
      }
      /**
       * (Re)deploy the url.  This will undeploy the url first, if it is
       * already deployed.  It also fetches
       */
      private void deploy() throws DeploymentException
      {
         if (deployed)
         {
            // already deployed... undeploy first
            getDeployerObj().undeploy(url);
         }
         getDeployerObj().deploy(url);
         
         // reset the watch url
         try
         {
            Object o = getServer().invoke(getDeployer(), "getWatchUrl",
            new Object[]
            { url },
            new String[]
            { URL.class.getName() });
            watchUrl = o == null ? url : (URL)o;
            
            getLog().debug("Watch URL for: " + url + " -> " + watchUrl);
         } catch (Exception e)
         {
            watchUrl = url;
            getLog().debug("Unable to obtain watchUrl from deployer.  " +
            "Use url: " + url, e);
         }
         
         // the watchurl may have changed... get a new lastModified
         lastModified = getLastModified();
         
         // set the deployed flag
         deployed = true;
      }
      /**
       * Undeploy the url (if deployed).
       */
      private void undeploy() throws DeploymentException
      {
         if (!deployed)
         {
            return;
         }
         getDeployerObj().undeploy(url);
         // reset the other fields
         deployed = false;
         lastModified = 0L;
         watchUrl = null;
      }
   }
   
   /**
    * This scanner scans a full directory instead of a single file.
    */
   private class DirScanner extends Scanner
   {
      /** the directory to scan */
      private File dir;
      
      /** the filter to use while scanning */
      private FileFilter filter;
      
      /** The comparator for deployment ordering */
      private Comparator comp;
      
      /** The list of currently deployed Scanners */
      private ArrayList deployed = new ArrayList();
      
      /** Set up the URL, filter, and comparator to use for this scanner */
      public DirScanner(URL url, Comparator comp, FileFilter filter)
      {
         super(url);
         if (!url.getProtocol().equals("file"))
         {
            throw new IllegalArgumentException("Urls for directory " +
            "scanning must use the file: protocol.");
         }
         dir = new File(url.getPath()).getAbsoluteFile();
         
         this.filter = filter == null ? defaultFilter : filter;
         this.comp = new FileComparator(comp == null ? defaultComparator : comp);
      }
      
      /**
       * Scan the directory for modifications / additions / removals.
       */
      public void scan()
      {
         // check the dir for existence and file-type
         if (!dir.exists())
         {
            getLog().warn("The director to scan does not exist: " + dir);
            return;
         }
         if (!dir.isDirectory())
         {
            getLog().warn("The directory to scan is not a directory: " + dir);
            return;
         }
         
         // get a sorted list of files in the directory
         File[] files;
         if (filter == null)
         {
            files = dir.listFiles();
         } else
         {
            files = dir.listFiles(filter);
         }
         Arrays.sort(files, comp);
         
         // step through the two sorted lists: files and deployed
         int deployedIndex = 0;
         int fileIndex = 0;
         
         while (true)
         {
            // get the current scanner and file
            Scanner scanner = null;
            if (deployedIndex < deployed.size())
            {
               scanner = (Scanner)deployed.get(deployedIndex);
            }
            File file = null;
            if (fileIndex < files.length)
            {
               file = files[fileIndex];
            }
            
            // if both scanner and file are null, we are done
            if (scanner == null && file == null)
            {
               break;
            }
            
            // determine if this is a new / old / or removed file, and
            // take the appropriate action
            switch (comp.compare(file, scanner == null ? null : scanner.url))
            {
               case -1: // the file is new.  Create the scanner
                  getLog().debug("Found new deployable application in directory " + 
                     dir + ": " + file);
                  try {
                     scanner = new Scanner(file.toURL());
                     deployed.add(deployedIndex, scanner);
                  } catch (MalformedURLException e) {
                     getLog().warn("Unable to obtain URL for file: " + file, e);
                     fileIndex++;
                  }
                  // do not break!  Intentionally fall through to normal scan.
               case 0: // the file is already deployed.  Scan it.
                  scanner.scan();
                  deployedIndex++;
                  fileIndex++;
                  break;
               case 1: // the file has been removed.  A normal scan will remove it
                  getLog().debug("Deployed application removed from directory " +
                     dir + ": " + file);
                  scanner.scan();
                  if (!scanner.deployed)
                  {
                     // the undeploy succeded... remove from deployed list
                     deployed.remove(deployedIndex);
                  } else
                  {
                     deployedIndex++;
                  }
                  break;
            }
         }
      }
   }
   
   /**
    * This comparator is used by the dirScanner.  It compares two url's
    * (or Files) using the specified urlComparator.  In the case of a tie,
    * it then uses File's natural ordering.  Finally, it normalizes all
    * compare values so that "less-than" is always -1, "greater-than" is
    * always 1, and "equals" is always 0.
    */
   private static class FileComparator implements Comparator
   {
      /** the delegated URL comparator */
      private Comparator urlComparator;
      
      /** Construct with a (possibly null) URL comparator */
      public FileComparator(Comparator urlComparator)
      {
         this.urlComparator = urlComparator;
      }
      
      /**
       * Compare all non-nulls as less than nulls.  Next, compare as URL's
       * using the delegated comparator.  And finally, use File's natural
       * ordering.
       */
      public int compare(Object o1, Object o2)
      {
         // catch nulls
         if (o1 == o2)
         {
            return 0;
         }
         if (o1 == null)
         {
            return 1;
         }
         if (o2 == null)
         {
            return -1;
         }
         
         // obtain the File and URL objects pertaining to each argument
         File file1;
         File file2;
         URL url1;
         URL url2;
         
         if (o1 instanceof URL)
         {
            url1 = (URL)o1;
            file1 = new File(url1.getPath());
         } else
         {
            file1 = (File)o1;
            try
            {
               url1 = file1.toURL();
            } catch (MalformedURLException e)
            {
               throw new IllegalStateException("Unable to create file url: " +
               file1 + ": " + e);
            }
         }
         if (o2 instanceof URL)
         {
            url2 = (URL)o2;
            file2 = new File(url2.getPath());
         } else
         {
            file2 = (File)o2;
            try
            {
               url2 = file2.toURL();
            } catch (MalformedURLException e)
            {
               throw new IllegalStateException("Unable to create file url: " +
               file2 + ": " + e);
            }
         }
         
         // first, use the delegate URL comparator
         int comp = 0;
         if (urlComparator != null)
         {
            comp = urlComparator.compare(url1, url2);
         }
         
         // If equal, break ties with File's natural ordering
         if (comp == 0)
         {
            comp = file1.compareTo(file2);
         }
         
         // normalize the comp value to -1, 0, 1
         return comp < 0 ? -1 : comp > 0 ? 1 : 0;
      }
   }
   
   /**
    * Scan all urls.
    */
   public void scan()
   {
      log.trace("Scanning urls...");
      
      // just scan all the scanners
      for (Iterator iter = scanners.iterator(); iter.hasNext(); )
      {
         ((Scanner)iter.next()).scan();
      }
   }
   
   /**
    * Obtain the Service values.  This was copied from Jason Dillons 
    * URLDeploymentScanner.  Thanks Jason!
    */
   public ObjectName preRegister(MBeanServer server, ObjectName name)
      throws Exception
   {
      
      // get server's home for relative paths, need this for setting
      // attribute final values, so we need todo it here
      serverHome = ServerConfigLocator.locate().getServerHomeDir();
      
      return super.preRegister(server, name);
   }
}
