package org.jboss.util.file;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileFilter;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.JarURLConnection;
import java.net.URL;
import java.net.URLConnection;
import java.util.jar.JarInputStream;
import java.util.jar.JarOutputStream;
import java.util.jar.Manifest;
import java.util.zip.ZipEntry;

/** A utility class for dealing with Jar files.

@author Scott.Stark@jboss.org
@version $Revision: 1.4.2.3 $
*/
public final class JarUtils
{
   /**
    * Hide the constructor
    */
   private JarUtils()
   {
   }
   
   /**
    * <P>This function will create a Jar archive containing the src
    * file/directory.  The archive will be written to the specified
    * OutputStream.</P>
    *
    * <P>This is a shortcut for<br>
    * <code>jar(out, new File[] { src }, null, null, null);</code></P>
    *
    * @param out The output stream to which the generated Jar archive is
    *        written.
    * @param src The file or directory to jar up.  Directories will be
    *        processed recursively.
    */
   public static void jar(OutputStream out, File src) throws IOException
   {
      jar(out, new File[] { src }, null, null, null);
   }
 
   /**
    * <P>This function will create a Jar archive containing the src
    * file/directory.  The archive will be written to the specified
    * OutputStream.</P>
    *
    * <P>This is a shortcut for<br>
    * <code>jar(out, src, null, null, null);</code></P>
    *
    * @param out The output stream to which the generated Jar archive is
    *        written.
    * @param src The file or directory to jar up.  Directories will be
    *        processed recursively.
    */
   public static void jar(OutputStream out, File[] src) throws IOException
   {
      jar(out, src, null, null, null);
   }
   
   /**
    * <P>This function will create a Jar archive containing the src
    * file/directory.  The archive will be written to the specified
    * OutputStream.  Directories are processed recursively, applying the
    * specified filter if it exists.
    *
    * <P>This is a shortcut for<br>
    * <code>jar(out, src, filter, null, null);</code></P>
    *
    * @param out The output stream to which the generated Jar archive is
    *        written.
    * @param src The file or directory to jar up.  Directories will be
    *        processed recursively.
    * @param filter The filter to use while processing directories.  Only
    *        those files matching will be included in the jar archive.  If
    *        null, then all files are included.
    */
   public static void jar(OutputStream out, File[] src, FileFilter filter)
      throws IOException
   {
      jar(out, src, filter, null, null);
   }
   
   /**
    * <P>This function will create a Jar archive containing the src
    * file/directory.  The archive will be written to the specified
    * OutputStream.  Directories are processed recursively, applying the
    * specified filter if it exists.
    *
    * @param out The output stream to which the generated Jar archive is
    *        written.
    * @param src The file or directory to jar up.  Directories will be
    *        processed recursively.
    * @param filter The filter to use while processing directories.  Only
    *        those files matching will be included in the jar archive.  If
    *        null, then all files are included.
    * @param prefix The name of an arbitrary directory that will precede all
    *        entries in the jar archive.  If null, then no prefix will be
    *        used.
    * @param man The manifest to use for the Jar archive.  If null, then no
    *        manifest will be included.
    */
   public static void jar(OutputStream out, File[] src, FileFilter filter,
      String prefix, Manifest man) throws IOException
   {
      
      for (int i = 0; i < src.length; i++)
      {
         if (!src[i].exists())
         {
            throw new FileNotFoundException(src.toString());
         }
      }
      
      JarOutputStream jout;
      if (man == null)
      {
         jout = new JarOutputStream(out);
      }
      else
      {
         jout = new JarOutputStream(out, man);
      }
      if (prefix != null && prefix.length() > 0 && !prefix.equals("/"))
      {
         // strip leading '/'
         if (prefix.charAt(0) == '/')
         {
            prefix = prefix.substring(1);
         }
         // ensure trailing '/'
         if (prefix.charAt(prefix.length() - 1) != '/')
         {
            prefix = prefix + "/";
         }
      } 
      else
      {
         prefix = "";
      }
      JarInfo info = new JarInfo(jout, filter);
      for (int i = 0; i < src.length; i++)
      {
         jar(src[i], prefix, info);
      }
      jout.close();
   }
   
   /**
    * This simple convenience class is used by the jar method to reduce the
    * number of arguments needed.  It holds all non-changing attributes
    * needed for the recursive jar method.
    */
   private static class JarInfo
   {
      public JarOutputStream out;
      public FileFilter filter;
      public byte[] buffer;
      
      public JarInfo(JarOutputStream out, FileFilter filter)
      {
         this.out = out;
         this.filter = filter;
         buffer = new byte[1024];
      }
   }
   
   /**
    * This recursive method writes all matching files and directories to
    * the jar output stream.
    */
   private static void jar(File src, String prefix, JarInfo info)
      throws IOException
   {
      
      JarOutputStream jout = info.out;
      if (src.isDirectory())
      {
         // create / init the zip entry
         prefix = prefix + src.getName() + "/";
         ZipEntry entry = new ZipEntry(prefix);
         entry.setTime(src.lastModified());
         entry.setMethod(JarOutputStream.STORED);
         entry.setSize(0L);
         entry.setCrc(0L);
         jout.putNextEntry(entry);
         jout.closeEntry();
         
         // process the sub-directories
         File[] files = src.listFiles(info.filter);
         for (int i = 0; i < files.length; i++)
         {
            jar(files[i], prefix, info);
         }
      } 
      else if (src.isFile())
      {
         // get the required info objects
         byte[] buffer = info.buffer;
         
         // create / init the zip entry
         ZipEntry entry = new ZipEntry(prefix + src.getName());
         entry.setTime(src.lastModified());
         jout.putNextEntry(entry);
         
         // dump the file
         FileInputStream in = new FileInputStream(src);
         int len;
         while ((len = in.read(buffer, 0, buffer.length)) != -1)
         {
            jout.write(buffer, 0, len);
         }
         in.close();
         jout.closeEntry();
      }
   }
   
   /**
    * Dump the contents of a JarArchive to the dpecified destination.
    */
   public static void unjar(InputStream in, File dest) throws IOException
   {
      if (!dest.exists())
      {
         dest.mkdirs();
      }
      if (!dest.isDirectory())
      {
         throw new IOException("Destination must be a directory.");
      }
      JarInputStream jin = new JarInputStream(in);
      byte[] buffer = new byte[1024];
      
      ZipEntry entry = jin.getNextEntry();
      while (entry != null)
      {
         String fileName = entry.getName();
         if (fileName.charAt(fileName.length() - 1) == '/')
         {
            fileName = fileName.substring(0, fileName.length() - 1);
         }
         if (fileName.charAt(0) == '/')
         {
            fileName = fileName.substring(1);
         }
         if (File.separatorChar != '/')
         {
            fileName = fileName.replace('/', File.separatorChar);
         }
         File file = new File(dest, fileName);
         if (entry.isDirectory())
         {
            // make sure the directory exists
            file.mkdirs();
            jin.closeEntry();
         } 
         else
         {
            // make sure the directory exists
            File parent = file.getParentFile();
            if (parent != null && !parent.exists())
            {
               parent.mkdirs();
            }
            
            // dump the file
            OutputStream out = new FileOutputStream(file);
            int len = 0;
            while ((len = jin.read(buffer, 0, buffer.length)) != -1)
            {
               out.write(buffer, 0, len);
            }
            out.flush();
            out.close();
            jin.closeEntry();
            file.setLastModified(entry.getTime());
         }
         entry = jin.getNextEntry();
      }
      /* Explicity write out the META-INF/MANIFEST.MF so that any headers such
      as the Class-Path are see for the unpackaged jar
      */
      Manifest mf = jin.getManifest();
      if (mf != null)
      {
         File file = new File(dest, "META-INF/MANIFEST.MF");
         File parent = file.getParentFile();
         if( parent.exists() == false )
         {
            parent.mkdirs();
         }
         OutputStream out = new FileOutputStream(file);
         mf.write(out);
         out.flush();
         out.close();
      }
      jin.close();
   }

   /** Given a URL check if its a jar url(jar:<url>!/archive) and if it is,
    extract the archive entry into the given dest directory and return a file
    URL to its location. If jarURL is not a jar url then it is simply returned
    as the URL for the jar.

    @param jarURL the URL to validate and extract the referenced entry if its
      a jar protocol URL
    @param dest the directory into which the nested jar will be extracted.
    @return the file: URL for the jar referenced by the jarURL parameter.
    */
   public static URL extractNestedJar(URL jarURL, File dest)
      throws IOException
   {
      // This may not be a jar URL so validate the protocol 
      if( jarURL.getProtocol().equals("jar") == false )
         return jarURL;

      String destPath = dest.getAbsolutePath();
      URLConnection urlConn = jarURL.openConnection();
      JarURLConnection jarConn = (JarURLConnection) urlConn;
      // Extract the archive to dest/jarName-contents/archive
      String parentArchiveName = jarConn.getJarFile().getName();
      // Find the longest common prefix between destPath and parentArchiveName
      int length = Math.min(destPath.length(), parentArchiveName.length());
      int n = 0;
      while( n < length )
      {
         char a = destPath.charAt(n);
         char b = parentArchiveName.charAt(n);
         if( a != b )
            break;
         n ++;
      }
      // Remove any common prefix from parentArchiveName
      parentArchiveName = parentArchiveName.substring(n);

      File archiveDir = new File(dest, parentArchiveName+"-contents");
      if( archiveDir.exists() == false && archiveDir.mkdirs() == false )
         throw new IOException("Failed to create contents directory for archive, path="+archiveDir.getAbsolutePath());
      String archiveName = jarConn.getEntryName();
      File archiveFile = new File(archiveDir, archiveName);
      File archiveParentDir = archiveFile.getParentFile();
      if( archiveParentDir.exists() == false && archiveParentDir.mkdirs() == false )
         throw new IOException("Failed to create parent directory for archive, path="+archiveParentDir.getAbsolutePath());
      InputStream archiveIS = jarConn.getInputStream();
      FileOutputStream fos = new FileOutputStream(archiveFile);
      BufferedOutputStream bos = new BufferedOutputStream(fos);
      byte[] buffer = new byte[4096];
      int read;
      while( (read = archiveIS.read(buffer)) > 0 )
      {
         bos.write(buffer, 0, read);
      }
      archiveIS.close();
      bos.close();

      // Return the file url to the extracted jar
      return archiveFile.toURL();
   }


   /**
    * A simple jar-like tool used for testing.  It's actually faster than 
    * jar, though doesn't sipport as many options.
    */
   public static void main(String[] args) throws Exception
   {
      if (args.length == 0)
      {
         System.out.println("usage: <x or c> <jar-archive> <files...>");
         System.exit(0);
      }
      if (args[0].equals("x"))
      {
         BufferedInputStream in = new BufferedInputStream(new FileInputStream(args[1]));
         File dest = new File(args[2]);
         unjar(in, dest);
      }
      else if (args[0].equals("c"))
      {
         BufferedOutputStream out = new BufferedOutputStream(new FileOutputStream(args[1]));
         File[] src = new File[args.length - 2];
         for (int i = 0; i < src.length; i++)
         {
            src[i] = new File(args[2 + i]);
         }
         jar(out, src);
      }
      else
      {
         System.out.println("Need x or c as first argument");
      }
   }
}
