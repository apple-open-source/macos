/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.plugins;

import org.jboss.console.manager.interfaces.ManageableResource;
import org.jboss.console.manager.interfaces.ResourceTreeNode;
import org.jboss.console.manager.interfaces.TreeNode;
import org.jboss.console.plugins.helpers.AbstractPluginWrapper;

import javax.management.ObjectInstance;

/**
 * As the number of UCL can be very big, we use a real Java class which is far
 * faster than beanshell
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.2 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>2 janv. 2003 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */
public class UCLLister 
   extends AbstractPluginWrapper
{

   public UCLLister () { super(); }
   
   ResourceTreeNode createUCLSubResource (ObjectInstance instance) throws Exception
   {
      String uclName = instance.getObjectName().getKeyProperty ("UCL");
            
      return createResourceNode ( 
            "UCL " + uclName, // name
            "UCL with id " + uclName, // description
            "images/service.gif", // Icon URL
            "/jmx-console/HtmlAdaptor?action=inspectMBean&name=" + encode(instance.getObjectName().toString()), // Default URL
            null,
            null,
            null,
            instance.getObjectName().toString(),
            instance.getClassName () );
   }
   
   ResourceTreeNode[] createUCLSubResources ()  throws Exception
   {
      ObjectInstance[] insts = 
         getMBeansForClass("jmx.loading:*", 
            "org.jboss.mx.loading.UnifiedClassLoader3");
      
      ResourceTreeNode[] result = new ResourceTreeNode[insts.length];
      for (int i=0; i<result.length; i++)
      {
         result[i] = createUCLSubResource (insts[i]);
      }
      
      return result;                  
   }
   
   protected TreeNode getTreeForResource(String profile, ManageableResource resource)
   {
      try
      {
         return createTreeNode (
               "Unified ClassLoaders", // name
               "Display all JBoss UCLs", // description
               "images/recycle.gif", // Icon URL
               null, // Default URL
               null,
               null, // sub nodes
               createUCLSubResources ()   // Sub-Resources                  
            );            
      }
      catch (Exception e)
      {
         e.printStackTrace ();
         return null;
      }
   }

  
}
