/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.deployment.scanner;

import java.io.UnsupportedEncodingException;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;
import java.util.StringTokenizer;

import org.jboss.deployment.IncompleteDeploymentException;
import org.jboss.deployment.NetBootFile;
import org.jboss.deployment.NetBootHelper;
import org.jboss.system.server.ServerConfig;
import org.jboss.util.NullArgumentException;
import org.jboss.util.Strings;

/**
 * Implement a scanner for HTTP server with un- re- deploy features. To enable these
 * features, a "lister" is used on the server: it allows to list some arbitrary
 * directory and return an XML string containing the detail of the directory.
 *
 * The Filtering feature is not yet implemented (because it relies on the File class
 * which cannot be used in our case, or should be tricked)
 *
 * The class extends URLDeploymentScanner but it doesn't really extends its code
 * instead, it will delegate to it everything related to:
 *  - absolute http url that identify a single file, such as http://server/bla.jar
 *     WARNING: such explicit JARs are NOT re- un- deployable!
 *  - file: urls (must be identified as a file: URL in the MBean definition)
 *
 * Remote expanded directory are not supported.
 *
 * The scanner would be able to work with multiple different Lister (one for
 * each URL for example): the processing and data structure are already there.
 * Nevertheless, there is currently no way to indicate this explicitly (in the MBean
 * definition for example). We would have to build a new naming scheme such as
 * LISTER_URL#DOWNLOAD_URL#REMOTE_FOLDER_NAME
 *
 * @see org.jboss.deployment.scanner.URLDeploymentScanner
 * @see org.jboss.deployment.NetBootHelper
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>6th of november 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

/**
 *
 * @jmx:mbean extends="org.jboss.deployment.scanner.URLDeploymentScannerMBean"
 *
 */
public class HttpURLDeploymentScanner 
   extends URLDeploymentScanner
   implements HttpURLDeploymentScannerMBean
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   protected String defaultHttpDirectoryListerUrl = null;
   protected String httpDirectoryDownload = null;

   protected HttpLister defaultHttpLister = null;
   
   protected HashMap scannedHttpUrls = new HashMap (); // lister to array of HttpDeploymentFolder map
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public HttpURLDeploymentScanner () { super (); }
   
   // Public --------------------------------------------------------
   
   /**
    * Default URL to be used when listing files on a remote HTTP folder
    * If none is provided, the one found in jboss.netboot.listing.url is used
    * If the URL is X, the resulting URL that is used to list the content of folder
    * "foo" will be "Xdir=foo": the provided URL must support this naming convention
    *
    * @jmx:managed-attribute
    */
   public String getDefaultHttpDirectoryListerUrl ()
   {
      if (defaultHttpDirectoryListerUrl == null)
         defaultHttpDirectoryListerUrl = NetBootHelper.getDefaultListUrl ();
      
      return this.defaultHttpDirectoryListerUrl;
   }
   
   /**
    * @jmx:managed-attribute
    */
   public void setDefaultHttpDirectoryListerUrl (String url)
   {
      this.defaultHttpDirectoryListerUrl = url;
   }
   
   /**
    * Default URL to be used when downloading files from a remote HTTP folder
    * If none is provided, the one found in jboss.server.home.url is used
    *
    * @jmx:managed-attribute
    */
   public String getDefaultHttpDirectoryDownloadUrl ()
   {
      if (httpDirectoryDownload == null)
         httpDirectoryDownload = NetBootHelper.getDefaultDownloadUrl ();
      
      return this.httpDirectoryDownload;
   }
   
   /**
    * @jmx:managed-attribute
    */
   public void setDefaultHttpDirectoryDownloadUrl (String url)
   {
      this.httpDirectoryDownload = url;
   }
   
   /**
    * @jmx:managed-attribute
    */
   public void setURLList(final List list)
   {
      // shouldn't be called. TODO setURLs is enough for now
   }

   /**
    * @jmx:managed-attribute
    */
   public void setURLs(final String listspec) throws MalformedURLException
   {
      if (listspec == null)
         throw new NullArgumentException("listspec");
      
      boolean debug = log.isDebugEnabled();
      
      List fileList = new LinkedList();
      
      StringTokenizer stok = new StringTokenizer(listspec, ",");
      while (stok.hasMoreTokens())
      {
         String urlspec = stok.nextToken().trim();
         
         if (debug)
         {
            log.debug("Adding URL from spec: " + urlspec);
         }
         
         // here we split between file based URL (or absolute JARs) that will 
         // be directly managed by our
         // ancestor, and HTTP based URL that WE will manage
         //         
         if (urlspec.startsWith ("file:") || urlspec.startsWith ("http:"))
         {            
            URL url = makeURL(urlspec);
            if (debug) log.debug("File URL: " + url);
            fileList.add(url);
         }
         else
         {
            URL url = makeURL(urlspec);
            if (debug) log.debug("HTTP URL: " + url);
            
            addHttpDeployment (urlspec, this.getDefaultHttpDirectoryLister ());            
         }
      }
      
      // we call our father: he will still manage URLs that we don't
      // => processing is split!
      //
      super.setURLList(fileList);      
   }

   public synchronized void scan() throws Exception
   {
      
      // call our father first: we split the process
      //
      super.scan ();
      
      boolean trace = log.isTraceEnabled();
      
      // Scan for new deployements
      if (trace) log.trace("Scanning for new http deployments");

      // we deploy, for each lister, every deploy name
      synchronized (scannedHttpUrls)
      {
         // we may have several Listers...
         //
         Iterator listers = this.getAllDeploymentListers().iterator ();
         while (listers.hasNext ())
         {
            HttpLister lister = (HttpLister)listers.next ();
            
            // ...Each Lister may have a set of associated folder to list...
            //
            Iterator deployments = this.getHttpDeploymentsForLister (lister).iterator ();
            while (deployments.hasNext ())
            {
               // ... And each folder possibly has a set of deployed files
               //
               HttpDeploymentFolder deploymentFolder = (HttpDeploymentFolder)deployments.next ();
               scanRemoteDirectory (deploymentFolder);
            }            
         }
      }
      
      // Now that all new files have been deployed, we 
      // scan for removed or changed deployments
      // we do it lister by lister to avoid to have an http read for each deployment
      // (instead we have one http read for each lister)
      //
      if (trace) log.trace("Scanning existing deployments for removal or modification");
      
      List removed = new LinkedList();
      List modified = new LinkedList();
      
      Iterator listers = this.getAllDeploymentListers().iterator ();
      while (listers.hasNext ())
      {
         HttpLister lister = (HttpLister)listers.next ();
         
         Iterator deployments = this.getHttpDeploymentsForLister (lister).iterator ();
         while (deployments.hasNext ())
         {
            HttpDeploymentFolder deploymentFolder = (HttpDeploymentFolder)deployments.next ();
            
            // get remote view for this lister/deployment folder couple
            //
            NetBootFile[] remoteFiles = NetBootHelper.listFilesFromDirectory (deploymentFolder.getCompleteListingUrl ());;
            
            Iterator deployedFiles = deploymentFolder.getDeployedFiles ().iterator ();
            while (deployedFiles.hasNext ())
            {
               DeployedRemoteURL deployed = (DeployedRemoteURL)deployedFiles.next ();
               
               NetBootFile alreadyDeployed = findFileWithName (deployed.getFile ().getName (), remoteFiles);
               
               if (alreadyDeployed == null)
               {
                  removed.add (deployed);
               }
               else if (alreadyDeployed.LastModified () > deployed.getFile ().LastModified ())
               {
                  deployed.updateFile (alreadyDeployed); // important! Required to update size and timestamp
                  modified.add (deployed);
               }
                              
            }
                                                                                              
         }            
      }
      
      for (Iterator iter=removed.iterator(); iter.hasNext();)
      {
         DeployedRemoteURL du = (DeployedRemoteURL)iter.next();
         undeploy(du);
      }
      
      for (Iterator iter=modified.iterator(); iter.hasNext();)
      {
         DeployedRemoteURL du = (DeployedRemoteURL)iter.next();
         undeploy(du);
         deploy(du);
      }

      // Validate that there are still incomplete deployments
      if( lastIncompleteDeploymentException != null )
      {
         try
         {
            Object[] args = {};
            String[] sig = {};
            Object o = getServer().invoke(getDeployer(),
               "checkIncompleteDeployments", args, sig);
         }
         catch (Exception e)
         {
            log.error(e);
         }
      }
   }

   
   

   protected void scanRemoteDirectory(HttpDeploymentFolder httpFolder) throws Exception
   {
      boolean trace = log.isTraceEnabled();
      
      if (trace) log.trace("Scanning directory: " + httpFolder.getRelativeFolder ());
      
      NetBootFile[] content = null;
      try
      {
         content = NetBootHelper.listFilesFromDirectory (httpFolder.getCompleteListingUrl ());
      }
      catch (Exception e)
      {         
         log.trace(e);
         return;
      }
     
      /*
       * TODO LATER
      File[] files = filter == null ? file.listFiles() : file.listFiles(filter);
      if (files == null)
      {
         throw new Exception("Null files returned from directory listing");
      }
       */
      
      // list of urls to deploy
      List list = new LinkedList();
      HashMap linkNameToObjects = new HashMap ();
      
      for (int i = 0; i < content.length; i++)
      {
         if (trace) log.trace("Checking deployment file: " + content[i]);         
         
         // Is it a new file
         //
         NetBootFile found = findFileWithName(content[i].getName (), httpFolder.getDeployedFiles());
         if (found == null)
         {
            URL target = httpFolder.getUrlForFile (content[i]);
            list.add(target);
            linkNameToObjects.put (target, content[i]);
         }
      }

      //
      // HACK, sort the elements so dependencies have a better chance of working
      //
      if (sorter != null)
         Collections.sort(list, sorter);
      
      // deploy each url
      Iterator iter = list.iterator();
      while (iter.hasNext())
      {
         URL url = (URL)iter.next();
         NetBootFile dep = (NetBootFile)linkNameToObjects.get (url);
         
         deploy( new DeployedRemoteURL(httpFolder, dep) );
      }
   }
   
   
   // Z implementation ----------------------------------------------
   
   // Y overrides ---------------------------------------------------
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   protected void undeploy(DeployedRemoteURL deployedUrl)
   {
      URL url = null;
      try
      {
         url = deployedUrl.getFolder ().getUrlForFile (deployedUrl.getFile ());

         if (log.isTraceEnabled()) log.trace("Undeploying: " + url);

         deployer.undeploy(url);
         
         deployedUrl.getFolder ().removeDeployedFile (deployedUrl);         
      }
      catch (Exception e)
      {
         log.error("Failed to undeploy: " + url, e);
      }
   }
   
   protected void deploy(DeployedRemoteURL deployedUrl) 
      throws MalformedURLException
   {
      
      URL url = deployedUrl.getFolder ().getUrlForFile (deployedUrl.getFile ());
      
      if( url == null ) return;

      if (log.isTraceEnabled()) log.trace("Deploying: " + url);
      
      try
      {
         deployer.deploy(url);
      }
      catch (IncompleteDeploymentException e)
      {
         lastIncompleteDeploymentException = e;
      }
      catch (Exception e)
      {
         log.error("Failed to deploy: " + url, e);
      }
      
      deployedUrl.getFolder ().addDeployedFile (deployedUrl);
   }
     
   // Find for file presence in a set of files
   //
   
   protected NetBootFile findFileWithName (String name, NetBootFile[] files)
   {
      for (int i=0; i<files.length; i++)
      {
         if (files[i].getName ().equals (name))
            return files[i];
      }
      return null;                  
   }   
   
   protected NetBootFile findFileWithName (String name, List deployedRemoteURL)
   {
      NetBootFile[] tmp = new NetBootFile[deployedRemoteURL.size ()];
      Iterator iter = deployedRemoteURL.iterator ();
      int i=0;
      while (iter.hasNext())
      {
         DeployedRemoteURL url = (DeployedRemoteURL)iter.next ();
         tmp[i] = url.getFile ();
         i++;
      }
      return findFileWithName(name, tmp);                  
   }   
   
   // Manage Lister and associated Directories that must be watched
   //
   
   protected synchronized void addHttpDeployment (String relativeName, HttpLister lister)
   {
      ArrayList deps = (ArrayList)scannedHttpUrls.get (lister);
      if (deps == null)
      {
         deps = new ArrayList();
         scannedHttpUrls.put (lister, deps);
      }
      deps.add (new HttpDeploymentFolder (relativeName, lister));
   }
   
   protected List getHttpDeploymentsForLister (HttpLister lister)
   {
      ArrayList deps = (ArrayList)scannedHttpUrls.get (lister);
      if (deps == null)
      {
         deps = new ArrayList();         
      }
      return deps;      
   }
   
   protected Set getAllDeploymentListers ()
   {
      return this.scannedHttpUrls.keySet ();      
   }
   
   /**
    * Default Lister object when no other lister is specified in the URLs
    */
   protected HttpLister getDefaultHttpDirectoryLister ()
   {
      if (defaultHttpLister == null)
         defaultHttpLister = new HttpLister (getDefaultHttpDirectoryDownloadUrl(), 
                                             getDefaultHttpDirectoryListerUrl());
      
      return this.defaultHttpLister;
   }
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------

   protected class HttpLister
   {
      public String downloadUrl = null;
      public String httpListerUrl = null;
      
      public HttpLister (String download, String list)
      {
         downloadUrl = download;
         httpListerUrl = list;
      }
      
      public String getDownloadUrl () { return this.downloadUrl; }
      public String getHttpListerUrl () { return this.httpListerUrl; }
      
      public int hashCode () { return this.httpListerUrl.hashCode (); }
      
      public boolean equals (Object obj)
      {
         if (obj instanceof HttpLister)
            return ((HttpLister)obj).httpListerUrl.equals (this.httpListerUrl);
         else
            return false;         
      }
      
   }
   
   protected class HttpDeploymentFolder
   {
      public String folder = null;
      public HttpLister myLister = null;
      public ArrayList deployedFiles = new ArrayList ();
      
      public HttpDeploymentFolder (String folder, HttpLister accessor)
      {
         this.folder = folder;
         this.myLister = accessor;
      }
      
      public String getRelativeFolder () { return this.folder; }
      public HttpLister getAssociatedLister () { return this.myLister; }
      
      public void addDeployedFile (DeployedRemoteURL file) { deployedFiles.add (file); }
      public void removeDeployedFile (DeployedRemoteURL file) { deployedFiles.remove (file); }
      
      public List getDeployedFiles () { return this.deployedFiles; }
      
      public String getCompleteListingUrl ()
         throws UnsupportedEncodingException
      {
         return NetBootHelper.buildListUrlForFolder (this.myLister.getHttpListerUrl (), this.folder);         
      }
      
      public URL getUrlForFile (NetBootFile file) 
         throws MalformedURLException
      {
         return new URL (NetBootHelper.buildDownloadUrlForFile (this.myLister.getDownloadUrl (), 
                                                                this.folder, 
                                                                file.getName ()));
      }
      
   }
    
   protected class DeployedRemoteURL
   {
      HttpDeploymentFolder folder = null;
      NetBootFile file = null;
      
      public DeployedRemoteURL (HttpDeploymentFolder folder, NetBootFile file)
      {
         this.folder = folder;
         this.file = file;
      }
      
      public HttpDeploymentFolder getFolder () { return this.folder; }
      public NetBootFile getFile () { return this.file; }
      
      public void updateFile (NetBootFile newer) { this.file = newer; }      
   }

}
