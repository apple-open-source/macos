/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.pm.file;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.net.URL;

import javax.jms.JMSException;
import org.jboss.mq.SpyJMSException;
import org.jboss.mq.SpyMessage;
import org.jboss.mq.server.MessageReference;
import org.jboss.system.ServiceMBeanSupport;

import org.jboss.system.server.ServerConfigLocator;

/**
 * This class manages the persistence needs of the MessageCache
 *
 * @jmx:mbean extends="org.jboss.system.ServiceMBean, org.jboss.mq.pm.CacheStoreMBean"
 *
 * @author     Hiram Chirino 
 * @version    $Revision: 1.8.2.1 $
 */
public class CacheStore
   extends ServiceMBeanSupport
   implements org.jboss.mq.pm.CacheStore, CacheStoreMBean
{
   String dataDirectory;
   File dataDir;

   /**
    * @see CacheStore#loadFromStorage(MessageReference)
    */
   public SpyMessage loadFromStorage(MessageReference mh) throws JMSException {
      try {
         File f = new File(dataDir, "Message-" + mh.referenceId);
         ObjectInputStream is = new ObjectInputStream(new BufferedInputStream(new FileInputStream(f)));
         Object rc = is.readObject();
         is.close();
         return (SpyMessage) rc;
      } catch (ClassNotFoundException e) {
         throw new SpyJMSException("Could not load message from secondary storage: ", e);
      } catch (IOException e) {
         throw new SpyJMSException("Could not load message from secondary storage: ", e);
      }
   }

   /**
    * @see CacheStore#saveToStorage(MessageReference, SpyMessage)
    */
   public void saveToStorage(MessageReference mh, SpyMessage message) throws JMSException {
      try {
         File f = new File(dataDir, "Message-" + mh.referenceId);
         ObjectOutputStream os = new ObjectOutputStream(new BufferedOutputStream(new FileOutputStream(f)));
         os.writeObject(message);
         os.close();
         mh.setStored(MessageReference.STORED);
      } catch (IOException e) {
         throw new SpyJMSException("Could not load message from secondary storage: ", e);
      }
   }

   /**
    * @see CacheStore#removeFromStorage(MessageReference)
    */
   public void removeFromStorage(MessageReference mh) throws JMSException {
      File f = new File(dataDir, "Message-" + mh.referenceId);
      f.delete();
      mh.setStored(MessageReference.NOT_STORED);
   }

   /**
    * Gets the DataDirectory attribute of the CacheStoreMBean object
    *
    * @return    The DataDirectory value
    *
    * @see CacheStoreMBean#getDataDirectory
    *
    * @jmx:managed-attribute
    */
   public String getDataDirectory() {
      return dataDirectory;
   }

   /**
    * Sets the DataDirectory attribute of the CacheStoreMBean object
    *
    * @param  newDataDirectory  The new DataDirectory value
    *
    * @see CacheStoreMBean#setDataDirectory
    *
    * @jmx:managed-attribute
    */
   public void setDataDirectory(String newDataDirectory) {
      dataDirectory = newDataDirectory;
   }

   /**
    * This gets called to start the service. 
    */
   protected void startService() throws Exception {
      boolean debug = log.isDebugEnabled();

      dataDir = null;
      // First check if the given Data Directory is a valid URL pointing to
      // a read and writable directory
      try {
         URL fileURL = new URL( dataDirectory );
         File file = new File( fileURL.getFile() );
         if( file.isDirectory() && file.canRead() && file.canWrite() ) {
            dataDir = file;
            if (debug)
               log.debug("Data directory set to: " + dataDir.getCanonicalPath());
         }
      }
      catch( Exception e ) {
         // Ignore message and try it as relative path
      }
      if( dataDir == null ) {
         // Get the system home directory
         File systemHomeDir = ServerConfigLocator.locate().getServerHomeDir();
   
         dataDir = new File(systemHomeDir, dataDirectory);
         if (debug)
            log.debug("Data directory set to: " + dataDir.getCanonicalPath());
   
         dataDir.mkdirs();
         if (!dataDir.isDirectory())
            throw new Exception("The configured data directory is not valid: " + dataDirectory);
      }

      // Clean out the directory of any previous files.
      File files[] = dataDir.listFiles();
      if (debug)
         log.debug("Removing " + files.length + " file(s) from: " + dataDir.getCanonicalPath());
      for (int i = 0; i < files.length; i++) {
         files[i].delete();
      }
   }

   /**
    * @see CacheStoreMBean#getCacheStoreInstance()
    */
   public Object getInstance() {
      return this;
   }

}
