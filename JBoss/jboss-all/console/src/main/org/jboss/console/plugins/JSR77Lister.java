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
import org.jboss.mx.util.MBeanProxy;

import javax.management.ObjectInstance;
import javax.management.ObjectName;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
/**
 * As the number of MBeans is very big, we use a real Java class which is far
 * faster than beanshell
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.4 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>2 janv. 2003 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */
public class JSR77Lister 
   extends AbstractPluginWrapper
{

   protected final static String JMX_JSR77_DOMAIN = "jboss.management.local";

   public JSR77Lister () { super(); }

   /*
   ResourceTreeNode[] createEARSubModules (ObjectName parent)  throws Exception
   {
      // there is a bug in the current jsr77 implementation with regard to naming
      // of EJBModule that are part of EARs => I've used a workaround
      //
      ObjectInstance[] insts = getMBeansForQuery(JMX_JSR77_DOMAIN + 
         ":j2eeType=EJBModule,J2EEServer="+parent.getKeyProperty("name")+",*", null);

      ResourceTreeNode[] jars = new ResourceTreeNode[insts.length];      
      for (int i=0; i<insts.length; i++)
      {
         ObjectName objName = insts[i].getObjectName();
         //EJBModule jarProxy = (EJBModule) 
         //   MBeanProxy.create(EJBModule.class, objName, getMBeanServer());
            
         jars[i] = createResourceNode(
               objName.getKeyProperty("name"),  // name
               "", // description
               null, // Icon URL
               null, // Default URL
               null,
               null, // sub nodes
               null,   // Sub-Resources                  
               objName.toString(),
               org.jboss.management.j2ee.EJBModule.class.getName()
            ).setVisibility(ResourceTreeNode.INVISIBLE_IF_SUBNODE_EXISTS);                  
         
      }
          
      return jars;  
   }
   
   ResourceTreeNode[] createEARs (ObjectName parent)  throws Exception
   {
      ObjectInstance[] insts = getMBeansForQuery(JMX_JSR77_DOMAIN + 
         ":j2eeType=J2EEApplication,J2EEServer="+parent.getKeyProperty("name")+",*", null);

      ResourceTreeNode[] ears = new ResourceTreeNode[insts.length];
      for (int i=0; i<insts.length; i++)
      {
         ObjectName objName = insts[i].getObjectName();
         //J2EEApplication earProxy = (J2EEApplication)
         //   MBeanProxy.create(J2EEApplication.class, objName, getMBeanServer());

         ears[i] = createResourceNode (
               objName.getKeyProperty("name"),  // name
               "", // description
               "images/EspressoMaker.gif", // Icon URL
               "J2EEApp.jsp?ObjectName=" + encode(objName.toString()), // Default URL
               null,
               null, // sub nodes
               createEARSubModules (objName),   // Sub-Resources
               parent.toString(),
               J2EEApplication.class.getName()
            );

      }
          
      return ears;  
   }
   
   ResourceTreeNode[] singleEJBs (ObjectName parent)  throws Exception
   {
      ObjectInstance[] insts = getMBeansForQuery(JMX_JSR77_DOMAIN + 
         ":j2eeType=EJBModule,J2EEServer="+parent.getKeyProperty("name")+",*", null);

      ResourceTreeNode[] jars = new ResourceTreeNode[insts.length];      
      for (int i=0; i<insts.length; i++)
      {
         ObjectName objName = insts[i].getObjectName();
         //EJBModule jarProxy = (EJBModule) 
         //   MBeanProxy.create(EJBModule.class, objName, getMBeanServer());
            
         jars[i] = createResourceNode(
               objName.getKeyProperty("name"),  // name
               "", // description
               null, // Icon URL
               null, // Default URL
               null,
               null, // sub nodes
               null,   // Sub-Resources                  
               objName.toString(),
               org.jboss.management.j2ee.EJBModule.class.getName()
            ).setVisibility(ResourceTreeNode.INVISIBLE_IF_SUBNODE_EXISTS);                  
         
      }
          
      return jars;  
   }
*/
   TreeNode createSubResources (ObjectName[] resources) throws Exception
   {
      ResourceTreeNode[] deployed = new ResourceTreeNode[resources.length];
      for (int i = 0; i < resources.length; i++)
      {
         deployed[i] = createResourceNode (
            resources[i].getKeyProperty ("name"), // name
            "J2EE Resource", // description
            null, //"images/EspressoMaker.gif", // Icon URL
            null, // "J2EEApp.jsp?ObjectName=" + encode (objName.toString ()), // Default URL
            null,
            null, // sub nodes
            null, //createEARSubModules (objName), // Sub-Resources
            resources[i].toString (),
            this.mbeanServer.getMBeanInfo (resources[i]).getClassName ()
         ).setVisibility (ResourceTreeNode.INVISIBLE_IF_SUBNODE_EXISTS);
      }

      return createTreeNode (
         "J2EE Resources", // name
         "J2EE Resources", // description
         "images/spirale.gif", // Icon URL
         null, //"J2EEDomain.jsp&objectName=" + encode(objName.toString()), // Default URL
         null,
         null, // sub nodes
         deployed   // Sub-Resources
      );
   }

   ResourceTreeNode[] createDeployedObjects (ObjectName[] resources) throws Exception
   {
      ArrayList deployed = new ArrayList ();
      for (int i = 0; i < resources.length; i++)
      {
         //if (resources[i].getKeyProperty ("J2EEApplication") == null)
         {
            deployed.add(createResourceNode (
               resources[i].getKeyProperty("name"), // name
               "", // description
               "images/EspressoMaker.gif", // Icon URL
               null, // "J2EEApp.jsp?ObjectName=" + encode (objName.toString ()), // Default URL
               null,
               null, // sub nodes
               null, //createEARSubModules (objName), // Sub-Resources
               resources[i].toString (),
               this.mbeanServer.getMBeanInfo (resources[i]).getClassName ()
            ).setVisibility (ResourceTreeNode.INVISIBLE_IF_SUBNODE_EXISTS));
         }

      }
      Collections.sort(deployed, new ListerSorter());

      return (ResourceTreeNode[])deployed.toArray(new ResourceTreeNode[deployed.size()]);
   }

   ResourceTreeNode createServer (ObjectName serverName) throws Exception
   {
      org.jboss.management.j2ee.J2EEServerMBean serv = (org.jboss.management.j2ee.J2EEServerMBean)
         MBeanProxy.get (org.jboss.management.j2ee.J2EEServerMBean.class, serverName, getMBeanServer ());

      ObjectName[] deployedON = serv.getDeployedObjects();
      ResourceTreeNode[] subResArray = createDeployedObjects (deployedON);

      return createResourceNode (
         serv.getServerVendor () + " - " + serv.getServerVersion (), // name
         serverName.getKeyProperty ("name"), // description
         "images/database.gif", // Icon URL
         null, //"J2EEDomain.jsp?objectName=" + encode(objName.toString()), // Default URL
         null,
         new TreeNode[] {createSubResources (serv.getResources())}, // sub nodes
         subResArray, // Sub-Resources
         serverName.toString (),
         org.jboss.management.j2ee.J2EEServer.class.getName ()
      );

   }

   ResourceTreeNode[] createServers (ObjectName domain)  throws Exception
   {
      org.jboss.management.j2ee.J2EEDomainMBean dom = (org.jboss.management.j2ee.J2EEDomainMBean)
         MBeanProxy.get (org.jboss.management.j2ee.J2EEDomainMBean.class, domain, getMBeanServer ());

      ObjectName[] serversObjectNames = dom.getServers();

      ArrayList servers = new ArrayList();
      for (int i=0; i< serversObjectNames.length; i++)
      {
         servers.add(createServer (serversObjectNames[i]));
      }

      return (ResourceTreeNode[])servers.toArray (new ResourceTreeNode[servers.size()]);
   }
   /*
   TreeNode createGenericNode (String name, ObjectName on, Class clazz) throws Exception
   {
      return createResourceNode(name, name, null, null, null, null, null, on.toString(), clazz.toString());
   }
   */

   TreeNode createDomain (ObjectName domain) throws Exception
   {
      return createTreeNode (
         domain.getKeyProperty ("name"), // name
         "", // description
         "images/spirale.gif", // Icon URL
         null, //"J2EEDomain.jsp&objectName=" + encode(objName.toString()), // Default URL
            null,
         null, // sub nodes
         createServers (domain)   // Sub-Resources
      );
   }

   TreeNode[] createDomains ()  throws Exception
   {      
      ObjectInstance[] insts = getMBeansForQuery(JMX_JSR77_DOMAIN + ":j2eeType=J2EEDomain,*", null);
      
      TreeNode[] domains = new TreeNode[insts.length];
      for (int i=0; i<insts.length; i++)
      {
         domains[i]= createDomain (insts[i].getObjectName ());
      }

      return domains;
   }
   
   protected TreeNode getTreeForResource(String profile, ManageableResource resource)
   {
      try
      {
         return createTreeNode (
               "J2EE Domains",  // name
               "Display JSR-77 Managed Objects", // description
               "images/elements32.gif", // Icon URL
               null, // Default URL
               null,
               createDomains (), // sub nodes
               null   // Sub-Resources                  
            );            
      }
      catch (Exception e)
      {
         e.printStackTrace ();
         return null;
      }
   }

   public final static String[] DEFAULT_SUFFIX_ORDER = {
      "ear", "jar", "war", "sar", "rar", "ds.xml", "service.xml", "wsr", "zip"
   };

   public class ListerSorter implements Comparator
   {

      protected String[] suffixOrder;

      public ListerSorter (String[] suffixOrder)
      {
         this.suffixOrder = suffixOrder;
      }

      public ListerSorter ()
      {
         this (DEFAULT_SUFFIX_ORDER);
      }

      public int compare (Object o1, Object o2)
      {
         return getExtensionIndex ((ResourceTreeNode) o1) - getExtensionIndex ((ResourceTreeNode) o2);
      }

      public int getExtensionIndex (ResourceTreeNode node)
      {
         String name = node.getName();
         if (name == null) name = "";

         int i = 0;
         for (; i < suffixOrder.length; i++)
         {
            if (name.endsWith (suffixOrder[i]))
               break;
         }
         return i;
      }
   }


}
