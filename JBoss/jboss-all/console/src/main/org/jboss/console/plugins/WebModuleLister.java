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
import org.jboss.management.j2ee.WebModuleMBean;
import org.jboss.mx.util.MBeanProxy;

import javax.management.ObjectName;
/**
 * As the number of MBeans is very big, we use a real Java class which is far
 * faster than beanshell
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>2 janv. 2003 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */
public class WebModuleLister 
   extends AbstractPluginWrapper
{

   protected final static String JMX_JSR77_DOMAIN = "jboss.management.local";
   
   public WebModuleLister () { super(); }
      
   ResourceTreeNode[] createBeans (ObjectName parent)  throws Exception
   {
       WebModuleMBean wmProxy = (WebModuleMBean)
               MBeanProxy.get(WebModuleMBean.class, parent, getMBeanServer());

       ObjectName[] servletsObjectName = wmProxy.getServlets();

      ResourceTreeNode[] servlets = new ResourceTreeNode[servletsObjectName.length];
      for (int i=0; i< servletsObjectName.length; i++)
      {
         ObjectName servletName = servletsObjectName[i];
          String name = servletName.getKeyProperty("name");

          servlets[i] = createResourceNode(
                  name,  // name
               "'" + name + "' Servlet", // description
               "images/serviceset.gif", // Icon URL
               "Servlet.jsp?ObjectName=" + encode(servletName.toString()), // Default URL
               null,
               null, // sub nodes
               null,   // Sub-Resources
               servletName.toString(),
               org.jboss.management.j2ee.Servlet.class.getName()
            );                  
         
      }
          
      return servlets;
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
               "images/spirale.gif", // Icon URL
               "WebModule.jsp?ObjectName=" + encode(objName.toString()), // Default URL
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
