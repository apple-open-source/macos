/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.navtree;

import java.awt.Component;
import java.net.URL;
import java.util.HashMap;

import javax.swing.ImageIcon;
import javax.swing.JTree;
import javax.swing.tree.DefaultTreeCellRenderer;

/**
 * Tree cell rendered. Can display another icon if available in the
 * plugin description.
 *
 * @see org.jboss.console.navtree.AdminTreeBrowser
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>20 decembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class TreeCellRenderer extends DefaultTreeCellRenderer
{
   protected TreeContext ctx = null;
   protected static HashMap cache = new HashMap ();
   
   public TreeCellRenderer (TreeContext ctx)
   {
      super();
      this.ctx = ctx;
   }

   public Component getTreeCellRendererComponent(
                        JTree tree,
                        Object value,
                        boolean sel,
                        boolean expanded,
                        boolean leaf,
                        int row,
                        boolean hasFocus) {

      super.getTreeCellRendererComponent(
                     tree, value, sel,
                     expanded, leaf, row,
                     hasFocus);
      if (value instanceof NodeWrapper)
      {
         NodeWrapper node = (NodeWrapper)value;
         
         String targetUrl = node.getIconUrl ();
         ImageIcon img = (ImageIcon)cache.get( targetUrl );
         
         if (img != null)
         {
            setIcon (img);
         }
         else
         {
            URL target = null;                                    
            
            try { target = new URL(this.ctx.localizeUrl(targetUrl)); } catch (Exception ignored) {}
            
            if (target != null)
            {
               try
               {
                  img = new ImageIcon(target);
                  cache.put (targetUrl, img);
                  setIcon (img);                  
               }
               catch (Exception tobad) {}
            }
         }
         
         
         String desc = node.getDescription ();
         if (desc != null)
         {
            setToolTipText (desc);
         }
      }

      return this;
   }
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public TreeCellRenderer ()
   {
   }
   
   // Public --------------------------------------------------------
   
   // Z implementation ----------------------------------------------
   
   // Y overrides ---------------------------------------------------
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
