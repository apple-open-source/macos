/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.logging.appender;

import java.io.File;

import java.net.URL;
import java.net.MalformedURLException;

import org.apache.log4j.helpers.LogLog;

/** 
 * An extention of the default Log4j FileAppender which
 * will make the directory structure for the set log file. 
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author  <a href="mailto:jason@planet57.com">Jason Dillon</a>
 */
public class FileAppender
   extends org.apache.log4j.FileAppender
{
   public void setFile(final String filename)
   {
      FileAppender.Helper.makePath(filename);
      super.setFile(filename);
   }

   /**
    * A helper for FileAppenders.
    */
   public static class Helper
   {
      public static void makePath(final String filename)
      {
         File dir;

         try {
            URL url = new URL(filename.trim());
            dir = new File(url.getFile()).getParentFile();
         }
         catch (MalformedURLException e) {
            dir = new File(filename.trim()).getParentFile();
         }

         if (!dir.exists()) {
            boolean success = dir.mkdirs();
            if (!success) {
               LogLog.error("Failed to create directory structure: " + dir);
            }
         }
      }
   }
}
