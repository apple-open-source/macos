/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.manager.interfaces.impl;

import org.jboss.console.manager.interfaces.ManageableResource;
import org.jboss.console.manager.interfaces.TreeAction;
import org.jboss.console.manager.interfaces.TreeInfo;
import org.jboss.console.manager.interfaces.TreeNode;
import org.jboss.console.manager.interfaces.TreeNodeMenuEntry;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;

/**
 * <description>
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.3 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>December 16, 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class DefaultTreeInfo 
   implements TreeInfo
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   protected ManageableResource[] roots = null;      
   protected HashMap resources = new HashMap ();   
   protected TreeAction homeAction = null;
   protected String jbossVersion = null;
   protected long version = 0;
   protected TreeNodeMenuEntry[] rootMenus = new TreeNodeMenuEntry[0];
   protected String iconUrl = null;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public DefaultTreeInfo () 
   {
      Package jbossPackage = Package.getPackage("org.jboss");
      jbossVersion = jbossPackage.getImplementationTitle() + " " +
               jbossPackage.getImplementationVersion();

   }
   
   // Public --------------------------------------------------------
   
   public ManageableResource[] getRootResources ()
   {
      return this.roots;
   }
   
   public void setRootResources (ManageableResource[] roots)
   {
      this.roots = roots;
   }
   
   public synchronized TreeNode[] getTreesForResource (ManageableResource resource)
   {
      ArrayList content = (ArrayList)resources.get (resource);
      if (content == null || content.size () == 0)
         return null;
      else
      {
         TreeNode[] result = new TreeNode[content.size ()];
         return (TreeNode[])content.toArray (result);
      }
   }
   
   public synchronized void addTreeToResource (ManageableResource resource, TreeNode tree)
   {
      ArrayList content = (ArrayList)resources.get (resource);
      if (content == null || content.size () == 0)
      {
         content = new ArrayList ();
         resources.put (resource, content);
      }       
      
      if (!content.contains (tree))
         content.add (tree);
   }
   
   public TreeAction getHomeAction ()
   {
      return this.homeAction;      
   }
      
   public void setHomeAction (TreeAction homeAction)
   {
      this.homeAction = homeAction;
   }
   
   public String getDescription ()
   {
      return jbossVersion;
   }   
      
   public void setRootMenus (TreeNodeMenuEntry[] menus)
   {
      this.rootMenus = menus;
   }
   
   public TreeNodeMenuEntry[] getRootMenus ()
   {
      return this.rootMenus;
   }

    // Z implementation ----------------------------------------------
   
   // Object overrides -----------------------------------------------
   
   public String toString ()
   {
      String result = "Root: " + roots + "\n" ;
      
      Iterator iter = resources.keySet ().iterator ();
      while (iter.hasNext ())
      {         
         ManageableResource key = (ManageableResource)iter.next();
         ArrayList content = (ArrayList)resources.get (key);
         
         result+="  Key: " + key + "\n";
         
         for (int i = 0; i < content.size(); i++)
         {
            result += "    Value: " + content.get(i);
         }
      
         result+="  ----\n";

      }
      return result;
   }   
   
   public long getTreeVersion ()
   {
      return this.version;
   }
   
   public void setTreeVersion (long version)
   {
      this.version = version;
   }
   
   public String getIconUrl ()
   {
      return this.iconUrl;
   }
   
   public void setIconUrl (String url)
   {
      this.iconUrl = url;
   }
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
