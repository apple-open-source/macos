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
import org.jboss.console.manager.interfaces.impl.MBeanResource;
import org.jboss.console.plugins.helpers.AbstractPluginWrapper;
import org.jboss.management.j2ee.J2EEApplicationMBean;
import org.jboss.mx.util.MBeanProxy;

import javax.management.ObjectName;
/**
 * As the number of MBeans is very big, we use a real Java class which is far
 * faster than beanshell
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.3 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>2 janv. 2003 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */
public class J2EEAppLister 
   extends AbstractPluginWrapper
{

   protected final static String JMX_JSR77_DOMAIN = "jboss.management.local";
   
   public J2EEAppLister () { super(); }
      
   ResourceTreeNode[] createModules (ObjectName[] modules)  throws Exception
   {
      ResourceTreeNode[] deployed = new ResourceTreeNode[modules.length];
      for (int i = 0; i < modules.length; i++)
      {
         //J2EEApplication earProxy = (J2EEApplication)
         //   MBeanProxy.create(J2EEApplication.class, objName, getMBeanServer());


         deployed[i] = createResourceNode (
            modules[i].getKeyProperty ("name"), // name
            "", // description
            "images/EspressoMaker.gif", // Icon URL
            null, // "J2EEApp.jsp?ObjectName=" + encode (objName.toString ()), // Default URL
            null,
            null, // sub nodes
            null, //createEARSubModules (objName), // Sub-Resources
            modules[i].toString (),
            this.mbeanServer.getMBeanInfo (modules[i]).getClassName ()
         ).setVisibility (ResourceTreeNode.INVISIBLE_IF_SUBNODE_EXISTS);

      }

      return deployed;
   }

   protected TreeNode getTreeForResource(String profile, ManageableResource resource)
   {
      try
      {
         ObjectName objName = ((MBeanResource)resource).getObjectName();
         J2EEApplicationMBean appProxy = (J2EEApplicationMBean)
            MBeanProxy.get (J2EEApplicationMBean.class, objName, getMBeanServer());

         return createTreeNode
            (
               objName.getKeyProperty("name"),  // name
               "", // description
               "images/EspressoMaker.gif", // Icon URL
               "J2EEApp.jsp?ObjectName=" + encode (objName.toString ()), // Default URL
               null,
               null, // sub nodes
               createModules (appProxy.getModules())   // Sub-Resources
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
