/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.deployment.cache;

import java.util.Map;
import java.util.HashMap;
import java.util.zip.CRC32;

import java.net.URL;
import java.net.URLConnection;
import java.net.MalformedURLException;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;

import java.io.InputStream;
import java.io.OutputStream;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;

import org.jboss.system.ServiceMBeanSupport;
import org.jboss.system.ConfigurationException;
import org.jboss.system.server.ServerConfigLocator;

import org.jboss.util.NullArgumentException;
import org.jboss.util.NestedRuntimeException;
import org.jboss.util.stream.Streams;

/**
 * A local file based {@link DeploymentStore}.
 *
 * @jmx:mbean extends="org.jboss.deployment.cache.DeploymentStoreMBean"
 *
 * @todo Validate the urlMap
 *
 * @version <tt>$Revision: 1.3 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class FileDeploymentStore
   extends ServiceMBeanSupport
   implements DeploymentStore, FileDeploymentStoreMBean
{
   /** The local directory where cache data will be stored. */
   protected File dir;

   /** The file where the mapping is located. */
   protected File mapFile;

   /** The URL to local file mapping. */
   protected Map urlMap;

   /**
    * Set the local directory where cache data will be stored.
    *
    * @param dir    The local directory where cache data will be stored.
    *
    * @throws IOException    File not found, not a directory, can't write...
    *
    * @jmx:managed-attribute
    */
   public void setDirectory(File dir) throws IOException
   {
      if (dir == null)
         throw new NullArgumentException("dir");

      if (!dir.isAbsolute()) {
         File serverHome = serverHome = ServerConfigLocator.locate().getServerHomeDir();
         dir = new File(serverHome, dir.getPath());
      }

      if (!dir.exists()) {
         if (!dir.mkdirs()) {
            throw new IOException("Failed to create directory: " + dir);
         }
      }

      if (!dir.isDirectory()) {
         throw new FileNotFoundException("Given file reference is not a directory: " + dir); 
      }

      if (!dir.canWrite()) {
         throw new IOException("Can not write to directory: " + dir);
      }

      if (!dir.canRead()) {
         throw new IOException("Can not read directory: " + dir);
      }

      // should be ok...

      this.dir = dir.getCanonicalFile();

      log.debug("Using directory for cache storage: " + dir);

      // the map file to use
      this.mapFile = new File(dir, "state-map.ser");
   }

   /**
    * Returns the local directory where cache data is stored.
    *
    * @return The local directory where cache data is stored.
    *
    * @jmx:managed-attribute
    */
   public File getDirectory() {
      return this.dir;
   }

   /**
    * Set the name of the local directory where cache data will be stored.
    *
    * <p>Invokes {@link #setDirectory}.
    *
    * @param dirname    The name of the local directory where cache data will be stored.
    *
    * @throws IOException    File not found, not a directory, can't write...
    *
    * @jmx:managed-attribute
    */
   public void setDirectoryName(String dirname) throws IOException
   {
      if (dirname == null)
         throw new NullArgumentException("dirname");

      setDirectory(new File(dirname));
   }

   /**
    * Get the name of the local directory where cache data is stored.
    *
    * @return The name of the local directory where cache data is stored.
    *
    * @jmx:managed-attribute
    */
   public String getDirectoryName() {
      return dir.getAbsolutePath();
   }


   /////////////////////////////////////////////////////////////////////////
   //                           DeploymentStore                           //
   /////////////////////////////////////////////////////////////////////////

   protected URL getURLFromFile(final File file)
   {
      try {
         return file.toURL();
      }
      catch (Exception e) {
         // should never happen
         throw new NestedRuntimeException(e);
      }
   }

   public URL get(final URL url) 
   {
      File file = (File)urlMap.get(url);
      if (file == null) return null;

      return getURLFromFile(file);
   }

   public URL put(final URL url) throws Exception 
   {
      URL localURL = get(url);
      File file;

      if (localURL == null) {
         // make a short unique filename for the given url which
         // will always be generated for this specific url
         CRC32 checksum = new CRC32();
         checksum.update(url.toString().getBytes());
      
         String prefix = Long.toString(checksum.getValue(), Character.MAX_RADIX);
         String filename = url.getFile();
         filename = filename.substring(filename.lastIndexOf("/") + 1, filename.length());
      
         file = new File(dir, prefix + "-" + filename);
         urlMap.put(url, file);
         writeMap();
      }
      else {
         // no need to regen the filename, we have it already
         file = (File)urlMap.get(url);
      }

      // copy the data from the url to the local file
      copyURL(url, file);

      // return the local file url
      return getURLFromFile(file);
   }

   /**
    * Copy the data at the given source URL to the given file.
    */
   protected void copyURL(final URL source, final File dest) throws IOException
   {
      InputStream is = new BufferedInputStream(source.openConnection().getInputStream());
      OutputStream os = new BufferedOutputStream(new FileOutputStream(dest));

      try {
         Streams.copy(is, os);
         os.flush();
      }
      finally {
         os.close();
         is.close();
      }
   }
      
   /**
    * Read the url map from serialized state.
    */
   protected Map readMap() throws ClassNotFoundException, IOException
   {
      if (mapFile.exists()) {
         Map map;

         InputStream is = new BufferedInputStream(new FileInputStream(mapFile));
         ObjectInputStream ois = new ObjectInputStream(is);

         try {
            map = (Map)ois.readObject();
         }
         finally {
            ois.close();
         }

         return map;
      }
      else {
         log.debug("Map file not found, creating new map");
         return new HashMap();
      }
   }

   /**
    * Write the url map to serialized state.
    */
   protected void writeMap() throws IOException
   {
      OutputStream os = new BufferedOutputStream(new FileOutputStream(mapFile));
      ObjectOutputStream oos = new ObjectOutputStream(os);

      try {
         oos.writeObject(urlMap);
         oos.flush();
      }
      finally {
         oos.close();
      }
   }


   /////////////////////////////////////////////////////////////////////////
   //                     Service/ServiceMBeanSupport                     //
   /////////////////////////////////////////////////////////////////////////

   /**
    * Setup the url map.
    */
   protected void createService() throws Exception
   {
      if (dir == null)
         throw new ConfigurationException("Missing attribute 'Directory'");

      // read the map if there is one
      urlMap = readMap();
   }
}
