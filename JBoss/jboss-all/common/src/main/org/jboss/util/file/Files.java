/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.util.file;

import java.io.File;
import java.io.IOException;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;

import org.jboss.logging.Logger;
import org.jboss.util.stream.Streams;

/**
 * A collection of file utilities.
 *
 * @version <tt>$Revision: 1.2.2.3 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 * @author Scott.Stark@jboss.org
 */
public final class Files
{
   private static final Logger log = Logger.getLogger(Files.class);
   /** The default size of the copy buffer. */
   public static final int DEFAULT_BUFFER_SIZE = 8192;

   /** Delete a file, or a directory and all of its contents.
    *
    * @param dir The directory or file to delete.
    * @return True if all delete operations were successfull.
    */
   public static boolean delete(final File dir)
   {
      boolean success = true;

      File files[] = dir.listFiles();
      if (files != null)
      {
         for (int i = 0; i < files.length; i++)
         {
            File f = files[i];
            if( f.isDirectory() == true )
            {
               // delete the directory and all of its contents.
               if( delete(f) == false )
               {
                  success = false;
                  log.debug("Failed to delete dir: "+f.getAbsolutePath());
               }
            }
            // delete each file in the directory
            else if( f.delete() == false )
            {
               success = false;
               log.debug("Failed to delete file: "+f.getAbsolutePath());
            }
         }
      }

      // finally delete the directory
      if( dir.delete() == false )
      {
         success = false;
         log.debug("Failed to delete dir: "+dir.getAbsolutePath());
      }

      return success;
   }

   /**
    * Delete a file or directory and all of its contents.
    *
    * @param dirname  The name of the file or directory to delete.
    * @return True if all delete operations were successfull.
    */
   public static boolean delete(final String dirname)
   {
      return delete(new File(dirname));
   }

   /**
    * Delete a directory contaning the given file and all its contents.
    *
    * @param filename a file or directory in the containing directory to delete
    * @return true if all delete operations were successfull, false if any
    * delete failed.
    */
   public static boolean deleteContaining(final String filename)
   {
      File file = new File(filename);
      File containingDir = file.getParentFile();
      return delete(containingDir);
   }

   /**
    * Copy a file.
    *
    * @param source  Source file to copy.
    * @param target  Destination target file.
    * @param buff    The copy buffer.
    *
    * @throws IOException  Failed to copy file.
    */
   public static void copy(final File source,
         final File target,
         final byte buff[])
         throws IOException
   {
      BufferedInputStream in = new BufferedInputStream(new FileInputStream(source));
      BufferedOutputStream out = new BufferedOutputStream(new FileOutputStream(target));

      int read;

      try
      {
         while ((read = in.read(buff)) != -1)
         {
            out.write(buff, 0, read);
         }
      }
      finally
      {
         Streams.flush(out);
         Streams.close(in);
         Streams.close(out);
      }
   }

   /**
    * Copy a file.
    *
    * @param source  Source file to copy.
    * @param target  Destination target file.
    * @param size    The size of the copy buffer.
    *
    * @throws IOException  Failed to copy file.
    */
   public static void copy(final File source,
         final File target,
         final int size)
         throws IOException
   {
      copy(source, target, new byte[size]);
   }

   /**
    * Copy a file.
    *
    * @param source  Source file to copy.
    * @param target  Destination target file.
    *
    * @throws IOException  Failed to copy file.
    */
   public static void copy(final File source, final File target)
         throws IOException
   {
      copy(source, target, DEFAULT_BUFFER_SIZE);
   }
}
