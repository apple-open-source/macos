/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/


package org.jboss.net.protocol.file;

import java.io.*;
import java.io.File;
import java.io.FileFilter;
import java.io.IOException;
import java.io.FileNotFoundException;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;

import org.jboss.net.protocol.URLListerBase;

public class FileURLLister extends URLListerBase
{
   public Collection listMembers (final URL baseUrl, final URLFilter filter) throws IOException   
   {
      return listMembers (baseUrl, filter, false);
   }

   public Collection listMembers (final URL baseUrl, final URLFilter filter, boolean scanNonDottedSubDirs) throws IOException
   {
      File directory = new File (baseUrl.getPath ());
      if (directory.exists () == false)
      {
         throw new FileNotFoundException (directory.toString ());
      }
      return listFiles (baseUrl, filter, scanNonDottedSubDirs);
      
   }
   
   protected Collection listFiles (URL baseUrl, final URLFilter filter, boolean scanNonDottedSubDirs)
   {      
      // List files at the current dir level
      //
      final File directory = new File (baseUrl.getPath ());      
      File[] files = directory.listFiles (new FileFilter ()
      {
         public boolean accept (File file)
         {
            try
            {
               return filter.accept (directory.toURL (), file.getName ());
            }
            catch (Exception forgetIt)
            {
               forgetIt.printStackTrace ();
               return true;
            }
         }
      });            
      
      // Do we have to scan sub-dirs as well?
      //
      if (scanNonDottedSubDirs)
      {
         ArrayList result = new ArrayList ();
         
         for (int i = 0; i < files.length; i++)
         {
            File file = files[i];
            
            if (doesDirTriggersRecursiveSearch (file))
            {  
               result.addAll (listFiles (prependDirToUrl (baseUrl, file.getName (), true), filter, scanNonDottedSubDirs));
            }
            else
            {               
               result.add ( fileToURL (baseUrl, file) );
            }
         }         
         
         return result;
      }
      else
      {
         // We don't scan sub-dirs ==> we simply add the URL to the result (after transformation)
         //
         return filesToURLs(baseUrl, files);
      }
            
   }
   
   protected boolean doesDirTriggersRecursiveSearch (File file)
   {
      // File is a directory and contains NO "." (which would means it 
      // could be an exploded package that needs to be deployed i.e. .war, etc.)
      //
      return file.isDirectory () && (file.getName ().indexOf (".") == -1);
   }
   
   protected URL prependDirToUrl (URL baseUrl, String addenda, boolean isDirectory)
   {
      try
      {
         String base = baseUrl.toString ();
         return new URL (base + (base.endsWith ("/") ? "" : "/" ) + addenda + (isDirectory ? "/" : "") );
      } 
      catch (MalformedURLException e)
      {
         // shouldn't happen
         throw new IllegalStateException ();
      }
   }
   
   protected Collection filesToURLs (final URL baseUrl, File[] files)
   {
      ArrayList result = new ArrayList (files.length);
      
      for (int i = 0; i < files.length; i++)
         result.add (fileToURL (baseUrl, files[i]));
      
      return result;
   }
      
   protected URL fileToURL (final URL baseUrl, File file)
   {
      return prependDirToUrl (baseUrl, file.getName (), file.isDirectory ());         
   }
}
