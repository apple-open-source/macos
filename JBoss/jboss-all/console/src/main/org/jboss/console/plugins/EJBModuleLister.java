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
import org.jboss.console.manager.interfaces.TreeNodeMenuEntry;
import org.jboss.console.manager.interfaces.impl.HttpLinkTreeAction;
import org.jboss.console.manager.interfaces.impl.MBeanResource;
import org.jboss.console.manager.interfaces.impl.SimpleTreeNodeMenuEntryImpl;
import org.jboss.console.plugins.helpers.AbstractPluginWrapper;

import javax.management.ObjectInstance;
import javax.management.ObjectName;
/**
 * As the number of MBeans is very big, we use a real Java class which is far
 * faster than beanshell
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.3.2.3 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>2 janv. 2003 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */
public class EJBModuleLister 
   extends AbstractPluginWrapper
{

   protected final static String JMX_JSR77_DOMAIN = "jboss.management.local";
   
   public EJBModuleLister () { super(); }
      
   ResourceTreeNode[] createBeans (ObjectName parent)  throws Exception
   {
      // there is a bug in the current jsr77 implementation with regard to naming
      // of EJBModule that are part of EARs => I've used a workaround
      //
      ObjectInstance[] insts = getMBeansForQuery(JMX_JSR77_DOMAIN + 
         ":EJBModule="+parent.getKeyProperty("name")+",*", null);

      ResourceTreeNode[] ejbs = new ResourceTreeNode[insts.length];      
      for (int i=0; i<insts.length; i++)
      {
         ObjectName objName = insts[i].getObjectName();
         String type = objName.getKeyProperty("j2eeType");

         String ejbName = objName.getKeyProperty("name");
         String containerUrl = "jboss.j2ee:service=EJB,jndiName=" + ejbName;
         containerUrl = java.net.URLEncoder.encode(containerUrl);
         containerUrl = "/jmx-console/HtmlAdaptor?action=inspectMBean&name=" + containerUrl;

         TreeNodeMenuEntry[] menus = new TreeNodeMenuEntry[]
            {
               new SimpleTreeNodeMenuEntryImpl ("View container in other window", 
                  new HttpLinkTreeAction (containerUrl, "_blank")
               )
            };
            
         String j2eeType = objName.getKeyProperty ("j2eeType");   
         String filename = "EJB.jsp";
         if (j2eeType.equalsIgnoreCase ("StatelessSessionBean"))
         {
            filename = "StatelessEjb.jsp";
         }
         else if (j2eeType.equalsIgnoreCase ("StatefulSessionBean"))
         {
            filename = "StatefulEjb.jsp";
         }
         else if (j2eeType.equalsIgnoreCase ("EntityBean"))
         {
            filename = "EntityEjb.jsp";
         }
         else if (j2eeType.equalsIgnoreCase ("MessageDrivenBean"))
         {
            filename = "MdbEjb.jsp";
         }
         
         ejbs[i] = createResourceNode(
               ejbName,  // name
               type, // description
               "images/bean.gif", // Icon URL
               filename+"?ObjectName=" + encode(objName.toString()), // Default URL
               menus,
               null, // sub nodes
               null,   // Sub-Resources                  
               objName.toString(), 
               insts[i].getClassName()
            );                  
         
      }
          
      return ejbs;  
   }

   protected TreeNode getTreeForResource(String profile, ManageableResource resource)
   {
      try
      {
         ObjectName objName = ((MBeanResource)resource).getObjectName();

         return createTreeNode
            (
               objName.getKeyProperty("name"),  // name
               "", // description
               "images/beans.gif", // Icon URL
               "EJBModule.jsp?ObjectName=" + encode(objName.toString()), // Default URL
               null,
               null, // sub nodes
               createBeans (objName)   // Sub-Resources                  
            ).setMasterNode(true);                  
         
      }
      catch (Exception e)
      {
         e.printStackTrace ();
         System.out.println (checker);
         return null;
         
      }
   }

}
