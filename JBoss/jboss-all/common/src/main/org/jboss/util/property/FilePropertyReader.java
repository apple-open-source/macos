/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.property;

import java.util.Properties;
import java.util.Map;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.BufferedInputStream;
import java.io.IOException;

import java.net.URL;

import org.jboss.util.NullArgumentException;

/**
 * Reads properties from one or more files.
 *
 * @version <tt>$Revision: 1.1.4.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class FilePropertyReader
   implements PropertyReader
{
   /** Array of filenames to load properties from */
   protected String[] filenames;

   /**
    * Construct a FilePropertyReader with an array of filenames
    * to read from.
    *
    * @param filenames  Filenames to load properties from
    */
   public FilePropertyReader(String[] filenames) {
      if (filenames == null)
         throw new NullArgumentException("filenames");

      this.filenames = filenames;
   }

   /**
    * Construct a FilePropertyReader with a single filename to read from.
    *
    * @param filename   Filename to load properties from
    */
   public FilePropertyReader(String filename) {
      this(new String[] { filename });
   }

   /**
    * Get an input stream for the given filename.
    *
    * @param filename   File name to get input stream for.
    * @return           Input stream for file.
    *
    * @throws IOException  Failed to get input stream for file.
    */
   protected InputStream getInputStream(String filename) throws IOException {
      File file = new File(filename);
      return new FileInputStream(file);
   }

   /**
    * Load properties from a file into a properties map.
    *
    * @param props      Properties map to load properties into.
    * @param filename   Filename to read properties from.
    *
    * @throws IOException              Failed to load properties from filename.
    * @throws IllegalArgumentException Filename is invalid.
    */
   protected void loadProperties(Properties props, String filename)
      throws IOException
   {
      if (filename == null)
         throw new NullArgumentException("filename");
      if (filename.equals(""))
         throw new IllegalArgumentException("filename");

      InputStream in = new BufferedInputStream(getInputStream(filename));
      props.load(in);
      in.close();
   }

   /**
    * Read properties from each specified filename
    *
    * @return  Read properties
    *
    * @throws PropertyException    Failed to read properties.
    * @throws IOException          I/O error while reading properties.
    */
   public Map readProperties()
      throws PropertyException, IOException
   {
      Properties props = new Properties();
      
      // load each specified property file
      for (int i=0; i<filenames.length; i++) {
         loadProperties(props, filenames[i]);
      }

      return props;
   }
}
