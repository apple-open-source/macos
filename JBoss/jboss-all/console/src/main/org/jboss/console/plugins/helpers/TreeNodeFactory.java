/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.plugins.helpers;

import org.jboss.console.manager.interfaces.ManageableResource;
import org.jboss.console.manager.interfaces.ResourceTreeNode;
import org.jboss.console.manager.interfaces.TreeAction;
import org.jboss.console.manager.interfaces.TreeNode;
import org.jboss.console.manager.interfaces.TreeNodeMenuEntry;
import org.jboss.console.manager.interfaces.impl.HttpLinkTreeAction;
import org.jboss.console.manager.interfaces.impl.MBeanResource;
import org.jboss.console.manager.interfaces.impl.SeparatorTreeNodeMenuEntry;
import org.jboss.console.manager.interfaces.impl.SimpleResourceTreeNode;
import org.jboss.console.manager.interfaces.impl.SimpleTreeNode;
import org.jboss.console.manager.interfaces.impl.SimpleTreeNodeMenuEntryImpl;

import javax.management.ObjectName;

/**
 * <description>
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.2 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>31 dec 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */
public class TreeNodeFactory
{

   public final static int NAME = 0;
   public final static int DESCRIPTION = 1;
   public final static int ICON_URL = 2;
   public final static int DEFAULT_URL = 3;
   public final static int MENU_ENTRIES = 4;
   public final static int SUB_NODES = 5;
   public final static int SUB_RESOURCES = 6;
   public final static int MANAGEABLE_RESOURCES = 7;
   
   public static TreeNode createTreeNode (Object[] content) throws Exception
   {
      if (content.length != 7 && content.length != 8)
         throw new Exception ("Bad number of parameters");
         
   
      String name = (String)content[NAME];
      String description = (String)content[DESCRIPTION];
      String iconUrl = (String)content[ICON_URL];
      String defaultUrl = (String)content[DEFAULT_URL];
      
      TreeAction action = new HttpLinkTreeAction (defaultUrl);
      
      // menu entries
      //
      TreeNodeMenuEntry[] menuEntries = createTreeMenus ((Object[])content[MENU_ENTRIES]);
      
      // sub nodes
      //
      TreeNode[] subNodes = null;
      Object[] genericSubNodes = (Object[])content[SUB_NODES];
      if (genericSubNodes != null && genericSubNodes.length > 0)
      {
         subNodes = new TreeNode[genericSubNodes.length];
         for (int i=0; i< genericSubNodes.length; i++)
         {
            subNodes[i] = createTreeNode ( (Object[])genericSubNodes[i] );
         }         
      }
      else
      {
         subNodes = new TreeNode[0];
      }
      
      // sub resources nodes
      //
      ResourceTreeNode[] subResNodes = null;
      Object[] genericSubResNodes = (Object[])content[SUB_RESOURCES];
      if (genericSubResNodes != null && genericSubResNodes.length > 0)
      {
         subResNodes = new ResourceTreeNode[genericSubResNodes.length];
         for (int i=0; i< genericSubResNodes.length; i++)
         {
            subResNodes[i] = (ResourceTreeNode)createTreeNode ( (Object[])genericSubResNodes[i] );
         }         
      }
      else
      {
         subResNodes = new ResourceTreeNode[0];
      }                                    
      
      if ((content.length-1) == MANAGEABLE_RESOURCES)
      {
         // we are a resource tree node
         //
         ManageableResource res = createManageableResource (content[MANAGEABLE_RESOURCES]);                  
         return new SimpleResourceTreeNode (name, description, iconUrl, action, menuEntries, subNodes, subResNodes, res);
         
      }
      else
      {
         // we are not a resource tree node, but simply a tree node!
         //
         return new SimpleTreeNode (name, description, iconUrl, action, menuEntries, subNodes, subResNodes);
      }
      
   }
   
   public static ManageableResource createManageableResource (Object content) throws Exception
   {
      Object[] realContent = (Object[])content;
      return new MBeanResource (new ObjectName((String)realContent[0]), (String)realContent[1]);
   }   
   
   protected static TreeNodeMenuEntry[] createTreeMenus (Object[] content) throws Exception
   {
          
      TreeNodeMenuEntry[] menuEntries = null;
      
      if (content != null && content.length > 0)
      {
         menuEntries = new TreeNodeMenuEntry[content.length];
         int i=0;
         while (i< content.length)
         {
            if (content[i] == null)
            {
               menuEntries[i] = new SeparatorTreeNodeMenuEntry();
               i++;
            }
            else
            {
               String text = (String)content[i];
               TreeAction action = new HttpLinkTreeAction((String)content[i+1]);
               menuEntries[i] = new SimpleTreeNodeMenuEntryImpl ( text, action );
               i+=2;
            }
         }         
      }
      else
      {
         menuEntries = new TreeNodeMenuEntry[0];
      }
      return menuEntries;
   }
}
