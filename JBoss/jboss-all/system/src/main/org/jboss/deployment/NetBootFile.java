/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.deployment;

/**
 * Represents a file/directory representation read from a distant HTTP server
 *
 * @see org.jboss.deployment.NetBootHelper
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>7 novembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class NetBootFile
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   String name = null;
   long size = 0;
   long lastModified = 0;
   boolean isDirectory = false;
   String lister = null;

   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public NetBootFile () {}

   public NetBootFile (String name, long size, long lastModified, boolean isDir, String lister)
   {
      this.name = name;
      this.size = size;
      this.lastModified = lastModified;
      this.isDirectory = isDir;
      this.lister = lister;
   }
   
   // Public --------------------------------------------------------
   
   public String getName ()
   {
      return this.name;
   }
   
   public long getSize ()
   {
      return this.size;
   }
   
   public long LastModified ()
   {
      return this.lastModified;
   }
   
   public boolean isDirectory() 
   {
      return this.isDirectory;
   }
   
   public String getListerUrl ()
   {
      return this.lister;
   }
   
   // Z implementation ----------------------------------------------
   
   // Y overrides ---------------------------------------------------
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
