/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins;

import java.io.IOException;
import java.io.File;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;

import java.rmi.RemoteException;

import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.RemoveException;

import org.jboss.ejb.Container;
import org.jboss.ejb.StatefulSessionContainer;
import org.jboss.ejb.StatefulSessionPersistenceManager;
import org.jboss.ejb.StatefulSessionEnterpriseContext;

import org.jboss.system.server.ServerConfigLocator;
import org.jboss.system.ServiceMBeanSupport;

import org.jboss.util.id.UID;

/**
 * A file-based stateful session bean persistence manager.
 *
 * <p>
 * Reads and writes session bean objects to files by using the
 * standard Java serialization mechanism.
 * 
 * <p>
 * Passivated state files are stored under:
 * <tt><em>jboss-server-data-dir</em>/<em>storeDirectoryName</em>/<em>ejb-name</em>-<em>unique-id</em></tt>.
 *
 * <p>
 * Since ejb-name is not unique across deployments we generate a <em>unique-id</em> to make
 * sure that beans with the same EJB name do not collide.
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean"
 * 
 * @version <tt>$Revision: 1.40.2.2 $</tt>
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:marc.fleury@telkel.com">Marc Fleury</a>
 * @author <a href="mailto:sebastien.alborini@m4x.org">Sebastien Alborini</a>
 * @author <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class StatefulSessionFilePersistenceManager
   extends ServiceMBeanSupport
   implements StatefulSessionPersistenceManager, StatefulSessionFilePersistenceManagerMBean
{
   /** The default store directory name ("<tt>sessions</tt>"). */
   public static final String DEFAULT_STORE_DIRECTORY_NAME = "sessions";
   
   /** Our container. */
   private StatefulSessionContainer con;

   /**
    * The sub-directory name under the server data directory where
    * session data is stored.
    *
    * @see #DEFAULT_STORE_DIRECTORY_NAME
    * @see setStoreDirectoryName
    */
   private String storeDirName = DEFAULT_STORE_DIRECTORY_NAME;
   
   /** The base directory where sessions state files are stored for our container. */
   private File storeDir;

   /**
    * Enable purging leftover state files at create and destroy
    * time (default is true).
    */
   private boolean purgeEnabled = true;
   
   /**
    * Saves a reference to the {@link StatefulSessionContainer} for
    * its bean type.
    *
    * @throws ClassCastException  Container is not a StatefulSessionContainer.
    */
   public void setContainer(final Container con)
   {
      this.con = (StatefulSessionContainer)con;
   }

   //
   // jason: these properties are intended to be used when plugins/interceptors 
   //        can take configuration values (need to update xml schema and processors).
   //
   
   /**
    * Set the sub-directory name under the server data directory
    * where session data will be stored.
    *
    * <p>
    * This value will be appened to the value of
    * <tt><em>jboss-server-data-dir</em></tt>.
    *
    * <p>
    * This value is only used during creation and will not dynamically
    * change the store directory when set after the create step has finished.
    *
    * @jmx:managed-attribute
    *
    * @param dirName   A sub-directory name.
    */
   public void setStoreDirectoryName(final String dirName)
   {
      this.storeDirName = dirName;
   }

   /**
    * Get the sub-directory name under the server data directory
    * where session data is stored.
    *
    * @jmx:managed-attibute
    *
    * @see #setStoreDirectoryName
    *
    * @return A sub-directory name.
    */
   public String getStoreDirectoryName()
   {
      return storeDirName;
   }

   /**
    * Set the stale session state purge enabled flag.
    *
    * @jmx:managed-attibute
    *
    * @param flag   The toggle flag to enable or disable purging.
    */
   public void setPurgeEnabled(final boolean flag)
   {
      this.purgeEnabled = flag;
   }

   /**
    * Get the stale session state purge enabled flag.
    *
    * @jmx:managed-attibute
    *
    * @return  True if purge is enabled.
    */
   public boolean getPurgeEnabled()
   {
      return purgeEnabled;
   }

   /**
    * Returns the directory used to store session passivation state files.
    *
    * @jmx:managed-attibute
    * 
    * @return The directory used to store session passivation state files.
    */
   public File getStoreDirectory()
   {
      return storeDir;
   }
   
   /**
    * Setup the session data storage directory.
    *
    * <p>Purges any existing session data found.
    */
   protected void createService() throws Exception
   {
      boolean debug = log.isDebugEnabled();

      // Initialize the dataStore

      String ejbName = con.getBeanMetaData().getEjbName();

      // Get the system data directory
      File dir = ServerConfigLocator.locate().getServerTempDir();

      // Setup the reference to the session data store directory
      dir = new File(dir, storeDirName);
      // ejbName is not unique across all deployments, so use a unique token
      dir = new File(dir, ejbName + "-" + new UID().toString());
      storeDir = dir;
      
      if (debug) {
         log.debug("Storing sessions for '" + ejbName + "' in: " + storeDir);
      }

      // if the directory does not exist then try to create it
      if (!storeDir.exists()) {
         if (!storeDir.mkdirs()) {
            throw new IOException("Failed to create directory: " + storeDir);
         }
      }
      
      // make sure we have a directory
      if (!storeDir.isDirectory()) {
         throw new IOException("File exists where directory expected: " + storeDir);
      }

      // make sure we can read and write to it
      if (!storeDir.canWrite() || !storeDir.canRead()) {
         throw new IOException("Directory must be readable and writable: " + storeDir);
      }
      
      // Purge state session state files, should be none, due to unique directory
      purgeAllSessionData();
   }

   /**
    * Removes any state files left in the storgage directory.
    */
   private void purgeAllSessionData()
   {
      if (!purgeEnabled) return;
      
      log.debug("Purging all session data in: " + storeDir);
      
      File[] sessions = storeDir.listFiles();
      for (int i = 0; i < sessions.length; i++)
      {
         if (! sessions[i].delete()) {
            log.warn("Failed to delete session state file: " + sessions[i]);
         }
         else {
            log.debug("Removed stale session state: " + sessions[i]);
         }
      }
   }
   
   /**
    * Purge any data in the store, and then the store directory too.
    */
   protected void destroyService() throws Exception
   {
      // Purge data and attempt to delete directory
      purgeAllSessionData();

      // Nuke the directory too if purge is enabled
      if (purgeEnabled && !storeDir.delete()) {
         log.warn("Failed to delete session state storage directory: " + storeDir);
      }
   }
   
   /**
    * Make a session state file for the given instance id.
    */
   private File getFile(final Object id)
   {
      //
      // jason: may have to translate id into a os-safe string, though
      //        the format of UID is safe on Unix and win32 already...
      //
      
      return new File(storeDir, String.valueOf(id) + ".ser");
   }

   /**
    * @return  A {@link UID}.
    */
   public Object createId(StatefulSessionEnterpriseContext ctx)
      throws Exception
   {
      return new UID();
   }

   /**
    * Non-operation.
    */
   public void createdSession(StatefulSessionEnterpriseContext ctx)
      throws Exception
   {
      // nothing
   }
   
   /**
    * Restores session state from the serialized file & invokes
    * {@link SessionBean#ejbActivate} on the target bean.
    */
   public void activateSession(final StatefulSessionEnterpriseContext ctx)
         throws RemoteException
   {
      boolean debug = log.isDebugEnabled();
      if (debug) {
         log.debug("Attempting to activate; ctx=" + ctx);
      }
      
      Object id = ctx.getId();
      
      // Load state
      File file = getFile(id);
      if (debug) {
         log.debug("Reading session state from: " + file);
      }
      
      try {
         SessionObjectInputStream in = new SessionObjectInputStream(
               ctx, 
               new BufferedInputStream(new FileInputStream(file)));
      
         try {
            Object obj = in.readObject();
            log.debug("Session state: " + obj);
            
            ctx.setInstance(obj);
         }
         finally {
            in.close();
         }
      }
      catch (Exception e)
      {
         throw new EJBException("Could not activate; failed to " +
               "restore state", e);
      }

      removePassivated(id);
            
      // Instruct the bean to perform activation logic
      ((SessionBean)ctx.getInstance()).ejbActivate();
      
      if (debug) {
         log.debug("Activation complete; ctx=" + ctx);
      }
   }

   /**
    * Invokes {@link SessionBean#ejbPassivate} on the target bean and saves the
    * state of the session to a file.
    */
   public void passivateSession(final StatefulSessionEnterpriseContext ctx)
         throws RemoteException
   {
      boolean debug = log.isDebugEnabled();
      if (debug) {
         log.debug("Attempting to passivate; ctx=" + ctx);
      }

      // Instruct the bean to perform passivation logic
      ((SessionBean)ctx.getInstance()).ejbPassivate();
            
      // Store state
      
      File file = getFile(ctx.getId());
      if (debug) {
         log.debug("Saving session state to: " + file);
      }
         
      try {
         SessionObjectOutputStream out = new SessionObjectOutputStream(
               new BufferedOutputStream(new FileOutputStream(file)));

         Object obj = ctx.getInstance();
         if (debug) {
            log.debug("Writing session state: " + obj);
         }

         try {
            out.writeObject(obj);
         }
         finally {
            out.close();
         }
      }
      catch (Exception e)
      {
         throw new EJBException("Could not passivate; failed to save state", e);
      }
      
      if (debug) {
         log.debug("Passivation complete; ctx=" + ctx);
      }
   }

   /**
    * Invokes {@link SessionBean.ejbRemove} on the target bean.
    */
   public void removeSession(final StatefulSessionEnterpriseContext ctx)
      throws RemoteException, RemoveException
   {
      boolean debug = log.isDebugEnabled();
      if (debug) {
         log.debug("Attempting to remove; ctx=" + ctx);
      }
      
      // Instruct the bean to perform removal logic
      ((SessionBean)ctx.getInstance()).ejbRemove();

      if (debug) {
         log.debug("Removal complete; ctx=" + ctx);
      }
   }

   /**
    * Removes the saved state file (if any) for the given session id.
    */
   public void removePassivated(final Object id)
   {
      boolean debug = log.isDebugEnabled();
      
      File file = getFile(id);

      // only attempt to delete if the file exists
      if (file.exists()) {
         if (debug) {
            log.debug("Removing passivated state file: " + file);
         }
         
         if (!file.delete()) {
            log.warn("Failed to delete passivated state file: " + file);
         }
      }
   }
}
