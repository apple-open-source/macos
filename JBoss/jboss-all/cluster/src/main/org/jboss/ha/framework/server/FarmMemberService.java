/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.ha.framework.server;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.FileOutputStream;
import java.io.FileInputStream;
import java.io.ByteArrayOutputStream;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.Iterator;
import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.jboss.deployment.scanner.URLDeploymentScanner;
import org.jboss.ha.framework.interfaces.HAPartition;
import org.jboss.system.server.ServerConfig;
import org.jboss.system.server.ServerConfigLocator;
import org.jboss.ha.framework.server.FarmMemberServiceMBean.FileContent;

/** 
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @author <a href="mailto:bill@jboss.org">Bill Burke</a>
 * @version $Revision: 1.11.2.7 $
 *
 * <p><b>20021014 andreas schaefer:</b>
 * <ul>
 *   <li>Initial import
 * </ul>
 * <p><b>20020809 bill burke:</b>
 * <ul>
 *   <li>Rewrote as a Scanner instead.  Also on boot-up asks cluster for deployable files
 * </ul>
 */
public class FarmMemberService extends URLDeploymentScanner implements FarmMemberServiceMBean
{
   private MBeanServer mServer;
   protected ObjectName mClusterPartitionName = null;
   protected String mBackgroundPartition = "DefaultPartition";
   private File mTempDirectory;

   protected final static String SERVICE_NAME = "FarmMemberService";
   protected HashMap parentDUMap = new HashMap();
   
   protected ArrayList remotelyDeployed = new ArrayList ();
   protected ArrayList remotelyUndeployed = new ArrayList ();

   public String getPartitionName()
   {
      return mBackgroundPartition;
   }
   
   public void setPartitionName( String pPartitionName )
   {
      if( ( getState () != STARTED ) && ( getState () != STARTING ) )
      {
         mBackgroundPartition = pPartitionName;
      }
   }

   /** Backward compatibility, mapped to the URLs attribute of URLDeploymentScannerMBean
    * @deprecated
    */
   public void setFarmDeployDirectory(String urls)
      throws MalformedURLException
   {
      super.setURLs(urls);
   }

   /** Backward compatibility, but ignored as it does nothing.
    * @deprecated
    */
   public void setScannerName(String name)
   {
      log.warn("ScannerName does nothing");
   }

   // Service implementation ----------------------------------------

   public String getName()
   {
      return "Farm Member Service";
   }
   
   /**
    * Saves the MBeanServer reference, create the Farm Member Name and
    * add its Notification Listener to listen for Deployment / Undeployment
    * notifications from the {@link org.jboss.deployment.MainDeployer MainDeployer}.
    */
   public ObjectName preRegister( MBeanServer pServer, ObjectName pName )
      throws Exception
   {
      mServer = pServer;
      return super.preRegister(pServer, pName);
   }
   /**
    * Looks up the Server Config instance to figure out the
    * temp-directory and the farm-deploy-directory
    **/
   protected void createService() throws Exception
   {
      super.createService();
      ServerConfig lConfig = ServerConfigLocator.locate();
      mTempDirectory = lConfig.getServerTempDir();
      
      createUnexistingLocalDir ();
   }
   /**
    * Register itself as RPC-Handler to the HA-Partition
    * and add the farm deployment directory to the scanner
    **/
   protected void startService()
      throws Exception
   {
      mClusterPartitionName = new ObjectName( "jboss:service=" + mBackgroundPartition );
      
      log.debug( "registerRPCHandler" );
      HAPartition lHAPartition = (HAPartition) mServer.getAttribute(
         mClusterPartitionName,
         "HAPartition"
      );
      lHAPartition.registerRPCHandler( SERVICE_NAME, this );

      ArrayList response = lHAPartition.callMethodOnCluster(
         SERVICE_NAME,
         "farmDeployments",
         new Object[] {
         },
         true
      );

      log.debug("Found "+response.size()+" farmDeployments responses");
      for (int i = 0; i < response.size(); i++)
      {
         Object map = response.get(i);
         if ( map != null && map instanceof HashMap )
         {
            HashMap farmed = (HashMap) map;
            pullNewDeployments(lHAPartition, farmed);
         }
      }

      // scan before we enable the thread, so JBoss version shows up afterwards
      scannerThread.doScan();

      // enable scanner thread if we are enabled
      scannerThread.setEnabled(scanEnabled.get());
   }
   

   protected void pullNewDeployments(HAPartition partition, HashMap farmed) throws Exception
   {
      log.info("**** pullNewDeployments ****");
      Iterator it = farmed.keySet().iterator();
      while (it.hasNext())
      {
         String depName = (String)it.next();
         DeployedURL du = (DeployedURL)parentDUMap.get(depName);
         Date last = (Date)farmed.get(depName);
         if (du != null)
         {
            Date theLast = new Date(du.getFile().lastModified());
            if (!theLast.before(last))
            {
               continue;
            }
         }
         ArrayList files = partition.callMethodOnCluster(
            SERVICE_NAME,
            "getFarmedDeployment",
            new Object[] {
               depName
            },
            true
            );
         for (int i = 0; i < files.size(); i++)
         {
            Object obj = files.get(i);
            if (obj != null && obj instanceof FileContent)
            {
               FileContent content = (FileContent)obj;
               String parentName = depName.substring(0, depName.indexOf('/'));
               farmDeploy(parentName, content, last);
            }
         }
      }
   }

   protected File findParent(String parentName)
   {
      URL[] urls = (URL[]) urlList.toArray( new URL[] {} );
      for (int i = 0; i < urlList.size(); i++)
      {
         if (urls[i].getProtocol().equals("file"))
         {
            File file = new File(urls[i].getFile());
            if (file.isDirectory())
            {
               if (file.getName().equals(parentName)) return file;
            }
         }
      }
      return null;
   }

   public HashMap farmDeployments()
   {
      log.debug("farmDeployments request, parentDUMap.size="+parentDUMap.size());
      Iterator it = parentDUMap.keySet().iterator();
      HashMap farmed = new HashMap();
      while(it.hasNext())
      {
         String key = (String)it.next();
         DeployedURL du = (DeployedURL)parentDUMap.get(key);
         farmed.put(key, new Date(du.getFile().lastModified()));
      }
      return farmed;
   }
   
   public void farmDeploy( String parentName, FileContent file, Date date ) 
   {
      try 
      {
         File parent = findParent(parentName);
         if (parent == null) 
         {
            log.info("Could not find parent: " + parentName + " for deployment: " + file + ", data: " + date);
            return;
         }
         
         String fullName = parentName + "/" + file.mFile.getName();
         
         DeployedURL du = null;
         synchronized(parentDUMap)
         {
            du = (DeployedURL)parentDUMap.get(fullName);
         }
         boolean deployIt = false;
         if (du == null) 
         {
            deployIt = true;
         }
         else
         {
            Date lastChanged = new Date(du.getFile().lastModified());
            deployIt = lastChanged.before(date);
         }

         if (deployIt)
         {
            // we remember this deployment to avoid recursive farm calls!
            //
            synchronized (remotelyDeployed)
            {
               remotelyDeployed.add (fullName);
            }
            
            // Create File locally and use it
            File lFile = new File(mTempDirectory, file.mFile.getName() );
            FileOutputStream lOutput = new FileOutputStream( lFile );
            lOutput.write( file.mContent );
            lOutput.close();
            log.info( "farmDeployment(), deploy locally: " + lFile );
            // Adjust the date and move the file to /farm
            // but delete it first if already there
            File lFarmFile = new File( parent, file.mFile.getName() );
            if( lFarmFile.exists() ) {
               lFarmFile.delete();
            }
            lFile.setLastModified( date.getTime() );
            lFile.renameTo( lFarmFile );
         }
         else
         {
            log.info(file.mFile.getName() + " is already deployed by farm service on this node");
         }
      }
      catch( Exception e ) {
         logException( e );
      }
   }
   
   public void farmUndeploy(String parentName, String fileName)
   {
      try {
         // First check if file is already deployed
         log.info( "doUndeployment(), File: " + parentName + "/" + fileName);
         File parent = findParent(parentName);
         if (parent == null) 
         {
            log.info("Could not find parent: " + parentName + " for undeployment: " + fileName);
            return;
         }
         File deployed = new File(parent, fileName);
         if (deployed.exists())
         {            
            // we remember this undeployment to avoid recursive farm calls!
            //
            synchronized (remotelyUndeployed)
            {
               String fullName = parentName + "/" + fileName;
               remotelyUndeployed.add (fullName);
            }

            deployed.delete();
            log.info( "farmUndeployment(), removed file" + deployed );
         }
      }
      catch( Exception e ) {
         logException( e );
      }
   }
   
   protected FileContent getFileContent( File pFile ) 
   {
      InputStream lInput = null;
      try {
         // Create File ByteArray
         byte[] lBuffer = new byte[ 1024 ];
         lInput = new FileInputStream( pFile );
         ByteArrayOutputStream lOutput = new ByteArrayOutputStream();
         int j = 0;
         while( ( j = lInput.read( lBuffer ) ) > 0 ) {
            lOutput.write( lBuffer, 0, j );
         }
         return new FileContent( pFile, lOutput.toByteArray() );
      }
      catch( java.io.FileNotFoundException fnfe ) {
         logException( fnfe );
      }
      catch( IOException ioe ) {
         logException( ioe );
      }
      finally
      {
         try { lInput.close (); } catch (Exception ignored) {}
      }
      return null;
   }
   

   public FileContent getFarmedDeployment(String depName)
   {
      try
      {
         DeployedURL du = (DeployedURL)parentDUMap.get(depName);
         if (du != null)
         {
            File file = du.getFile();
            return getFileContent(file);
         }
         else
            return null; // this may be available only on some other node
      }
      catch (Exception ex)
      {
         logException(ex);
         return null;
      }
   }

   protected void deploy(final DeployedURL du)
   {
      super.deploy(du);
      File file = du.getFile();
      File parent = file.getParentFile();
      if (parent == null) return;
      
      String fullName = parent.getName() + "/" + file.getName();
      synchronized (parentDUMap)
      {
         parentDUMap.put(fullName, du);
      }

      if (getState() == STARTING) return;

      try
      {
         // We check if we must do a remote call or not: maybe the deploy 
         // is already the consequence of a farm call! (avoid recusivity!)
         //
         boolean consequenceOfRemoteCall = false;
         synchronized (remotelyDeployed)
         {
            consequenceOfRemoteCall = remotelyDeployed.remove (fullName);
         }
         
         if (!consequenceOfRemoteCall)
         {
            FileContent fileContent = getFileContent(file);
            Date fileDate = new Date(file.lastModified());
            HAPartition lHAPartition = (HAPartition) mServer.getAttribute(
               mClusterPartitionName,
               "HAPartition"
               );
            lHAPartition.callMethodOnCluster(
               SERVICE_NAME,
               "farmDeploy",
               new Object[] {
                  parent.getName(),
                  fileContent,
                  fileDate
               },
               true
               );
         }
      }
      catch (Exception ex)
      {
         logException(ex);
      }
   }

   protected void undeploy(final DeployedURL du)
   {
      
      File file = du.getFile();
      File parent = file.getParentFile();
      String parentName = parent.getName();
      String fileName = file.getName();
      super.undeploy(du);
      
      String fullName = parent.getName() + "/" + file.getName();
      synchronized (parentDUMap)
      {
         parentDUMap.remove(fullName);
      }
      
      if (getState() == STOPPING) return;

      try
      {
         // We check if we must do a remote call or not: maybe the undeploy 
         // is already the consequence of a farm call! (avoid recusivity!)
         //
         boolean consequenceOfRemoteCall = false;
         synchronized (remotelyUndeployed)
         {
            consequenceOfRemoteCall = remotelyUndeployed.remove (fullName);
         }
         
         if (!consequenceOfRemoteCall)
         {
            HAPartition lHAPartition = (HAPartition) mServer.getAttribute(
               mClusterPartitionName,
               "HAPartition"
               );
            lHAPartition.callMethodOnCluster(
               SERVICE_NAME,
               "farmUndeploy",
               new Object[] {
                  parentName,
                  fileName
               },
               true
               );
         }
      }
      catch (Exception ex)
      {
         logException(ex);
      }
   }

   /**
    * Go through the myriad of nested JMX exception to pull out the true
    * exception if possible and log it.
    *
    * @param e The exception to be logged.
    */
   private void logException( Throwable e )
   {
      if (e instanceof javax.management.RuntimeErrorException)
      {
         e = ((javax.management.RuntimeErrorException)e).getTargetError();
      }
      else if (e instanceof javax.management.RuntimeMBeanException)
      {
         e = ((javax.management.RuntimeMBeanException)e).getTargetException();
      }
      else if (e instanceof javax.management.RuntimeOperationsException)
      {
         e = ((javax.management.RuntimeOperationsException)e).getTargetException();
      }
      else if (e instanceof javax.management.MBeanException)
      {
         e = ((javax.management.MBeanException)e).getTargetException();
      }
      else if (e instanceof javax.management.ReflectionException)
      {
         e = ((javax.management.ReflectionException)e).getTargetException();
      }
      e.printStackTrace();
      log.error(e);
   }
   
   protected void createUnexistingLocalDir()
   {
      if (this.urlList != null)
      {
         Iterator iter = this.urlList.iterator ();
         while (iter.hasNext ())
         {
            URL url = null;
            try
            {
               url = (URL)iter.next ();
               if (url.getProtocol().equals("file"))
               {
                  File targetDir = new File (url.getFile ());
                  if (!targetDir.exists ())
                     targetDir.mkdirs ();
               }
            }
            catch (Exception e)
            {
               log.info ("Problem while creating a farm directory: " + url, e);
            }
         }
      }
   }
}
