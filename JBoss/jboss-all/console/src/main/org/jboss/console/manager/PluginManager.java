/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.manager;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;

import javax.management.MBeanServer;
import javax.management.Notification;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;
import javax.management.ObjectName;
import javax.naming.InitialContext;

import org.jboss.console.manager.interfaces.ConsolePlugin;
import org.jboss.console.manager.interfaces.ManageableResource;
import org.jboss.console.manager.interfaces.ResourceTreeNode;
import org.jboss.console.manager.interfaces.TreeInfo;
import org.jboss.console.manager.interfaces.TreeNode;
import org.jboss.console.manager.interfaces.TreeNodeMenuEntry;
import org.jboss.console.manager.interfaces.impl.DefaultTreeInfo;
import org.jboss.console.manager.interfaces.impl.HttpLinkTreeAction;
import org.jboss.console.manager.interfaces.impl.MBeanAction;
import org.jboss.console.manager.interfaces.impl.MBeanResource;
import org.jboss.console.manager.interfaces.impl.SeparatorTreeNodeMenuEntry;
import org.jboss.console.manager.interfaces.impl.SimpleTreeNodeMenuEntryImpl;
import org.jboss.console.navtree.RefreshTreeAction;
import org.jboss.jmx.adaptor.rmi.RMIRemoteMBeanProxy;
import org.jboss.system.Registry;
import org.jboss.system.ServiceMBeanSupport;

/**
 * Main service for the administration console: aggregate all information from
 * plugins that is needed for the tree to be displayed
 *
 * @see org.jboss.console.manager.interfaces.TreeInfo
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.3 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>14 decembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

/**
 * @jmx:mbean extends="org.jboss.system.ServiceMBean"
 */
public class PluginManager
   extends ServiceMBeanSupport
   implements PluginManagerMBean, NotificationListener
{
   
   // Constants -----------------------------------------------------
   
   public static String PLUGIN_MANAGER_NAME = null;
   
   // Attributes ----------------------------------------------------
   
   protected ArrayList plugins = new ArrayList ();
   protected ManageableResource bootstrapResource = null;

   public String jndiName = "console/PluginManager";
   
   protected long treeVersion = 0;
   protected HashMap currentTrees = new HashMap();
   
   protected String mainLogoUrl = "/web-console/images/jboss.gif";
   protected String mainLinkUrl = "http://www.jboss.org/forums/";
   protected boolean enableShutdown = true;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
   public PluginManager ()
   {
   }
   
   // Public --------------------------------------------------------
   
   public void createService () throws Exception
   {
      this.bootstrapResource = new MBeanResource (this.getServiceName (), this.getClass ().toString ());      
   }
   
   public void startService () throws Exception
   {
      bindProxyInJndi ();
      PLUGIN_MANAGER_NAME = this.getServiceName().toString();
      Registry.bind(PLUGIN_MANAGER_NAME, this);
      
      initNotificationReception ();      
   }
   
   public void stopService ()
   {
      Registry.unbind(this.getServiceName().toString());
   }

   /**
    * send a message
    * @jmx:managed-operation
    */
   public void registerPlugin (String consolePluginClassName) throws Exception
   {
      Class pluginClass = Thread.currentThread ().getContextClassLoader ().
                           loadClass (consolePluginClassName);
      ConsolePlugin plugin = (ConsolePlugin)pluginClass.newInstance ();
      this.registerPlugin (plugin);
   }
   
   /**
    * send a message
    * @jmx:managed-operation
    */
   public synchronized void registerPlugin (ConsolePlugin plugin)
   {
      plugins.add (plugin);
      regenerateAdminTree();
   }

   /**
    * send a message
    * @jmx:managed-operation
    */
   public synchronized void unregisterPlugin (ConsolePlugin plugin)
   {
      plugins.remove (plugin);
      regenerateAdminTree();
   }

   /**
    * @jmx:managed-operation
    */
   public synchronized void regenerateAdminTree ()
   {
      // remove all cached trees
      //
      currentTrees.clear();
   }
   
   /**
    * @jmx:managed-operation
    */
   public synchronized void regenerateAdminTreeForProfile (String profile)
   {
      // remove cached tree for profile (if any)
      //
      currentTrees.remove(profile);
   }
      
   /**
    * @jmx:managed-operation
    */
   public synchronized TreeInfo getTreeForProfile (String profile)
   {
      TreeInfo currentTree = (TreeInfo)currentTrees.get(profile);
      
      if (currentTree == null)
      {      
         HashSet resourcesToManage = new HashSet ();      
         TreeInfo result = new DefaultTreeInfo ();      
         ArrayList pluginsSubset = getPluginsSubsetForProfile (profile);
         HashSet resourcesAlreadyScanned = new HashSet ();
         
         result.setRootResources (new ManageableResource[] {bootstrapResource});
         
         // Bootstrap tree creation
         //
         resourcesToManage.add (bootstrapResource);
         
         while (resourcesToManage.size () > 0)
         {
            ManageableResource currentResource = (ManageableResource)resourcesToManage.iterator ().next ();
            
            // pre-clean resources environment
            //
            resourcesToManage.remove (currentResource);
            resourcesAlreadyScanned.add (currentResource);
            
            Iterator iter = getTreesForResource(currentResource, profile, pluginsSubset);
            while (iter.hasNext ())
            {
               TreeNode subTree = (TreeNode)iter.next ();
               result.addTreeToResource (currentResource, subTree);
               HashSet subResources = findSubResources (subTree);
               if (subResources != null && subResources.size () > 0)
               {
                  Iterator subsRes = subResources.iterator ();
                  while (subsRes.hasNext ())
                  {
                     ManageableResource subRes = (ManageableResource)subsRes.next ();
                     if (!resourcesAlreadyScanned.contains (subRes))
                        resourcesToManage.add (subRes);                  
                  }
               }
               
   
            }
         }
         
         this.treeVersion++;
         result.setTreeVersion (this.treeVersion);
         try
         {            
            TreeNodeMenuEntry[] base = new TreeNodeMenuEntry[]
            {
               new SimpleTreeNodeMenuEntryImpl ("Update tree", new RefreshTreeAction (false)),
               new SimpleTreeNodeMenuEntryImpl ("Force update tree", new RefreshTreeAction (true)),
            };
            
            if (enableShutdown)
            {
               result.setRootMenus (new TreeNodeMenuEntry[]
                  {
                     base[0],
                     base[1],
                     new SeparatorTreeNodeMenuEntry (),
                     new SimpleTreeNodeMenuEntryImpl ("Shutdown JBoss instance", 
                        new MBeanAction (new ObjectName("jboss.system:type=Server"),
                                          "shutdown", new Object[0], new String[0])
                     ),
                     new SimpleTreeNodeMenuEntryImpl ("Shutdown and Restart JBoss instance", 
                        new MBeanAction (new ObjectName("jboss.system:type=Server"),
                                          "exit", new Object[] {new Integer (10)}, 
                                          new String[] {"int"})                                       
                     ),
                     new SimpleTreeNodeMenuEntryImpl ("HALT and Restart JBoss instance", 
                        new MBeanAction (new ObjectName("jboss.system:type=Server"),
                                          "halt", new Object[] {new Integer (10)}, 
                                          new String[] {"int"})                                       
                     )
                  }
               );
            }
            else
            {
               result.setRootMenus (base);
            }
            
            result.setHomeAction(new HttpLinkTreeAction (this.mainLinkUrl));
            result.setIconUrl (this.mainLogoUrl);
         }
         catch (Exception bla) {}
         
         currentTree = result;
         
         currentTrees.put(profile, currentTree);
         
      }
      
      return currentTree;
   }
   
   /**
    * Only return the tree if the actual version is bigger than the known version
    * @jmx:managed-operation
    */
   public synchronized TreeInfo getUpdateTreeForProfile (String profile, long knownVersion)
   {
      TreeInfo currentTree = (TreeInfo)currentTrees.get(profile);
            
      if (this.treeVersion > knownVersion || currentTree==null)
         return getTreeForProfile (profile);
      else
         return null;
   }
   
   /**
    *@jmx:managed-attribute
    */
   public MBeanServer getMBeanServer()
   {
      return this.server;
   }
   
   /**
    *@jmx:managed-attribute
    */
   public ManageableResource getBootstrapResource()
   {
      return this.bootstrapResource;
   }
   
   /**
    *@jmx:managed-attribute
    */
   public String getJndiName()
   {
      return jndiName;
   }

   /**
    *@jmx:managed-attribute
    */
   public void setJndiName(String jndiName)
   {
      this.jndiName = jndiName;
   }

   /**
    *@jmx:managed-attribute
    */
   public boolean isEnableShutdown()
   {
      return enableShutdown;
   }

   /**
    *@jmx:managed-attribute
    */
   public void setEnableShutdown(boolean enableShutdown)
   {
      this.enableShutdown = enableShutdown;
      treeVersion++;
   }

   /**
    *@jmx:managed-attribute
    */
   public String getMainLinkUrl()
   {
      return mainLinkUrl;      
   }

   /**
    *@jmx:managed-attribute
    */
   public void setMainLinkUrl(String mainLinkUrl)
   {
      this.mainLinkUrl = mainLinkUrl;
      treeVersion++;
   }

   /**
    *@jmx:managed-attribute
    */
   public String getMainLogoUrl()
   {
      return mainLogoUrl;
   }

   /**
    *@jmx:managed-attribute
    */
   public void setMainLogoUrl(String mainLogoUrl)
   {
      this.mainLogoUrl = mainLogoUrl;
      treeVersion++;
   }

   // Z implementation ----------------------------------------------
   
   // NotificationListener implementation ----------------------------------------------
   
   public void handleNotification (Notification notif, Object handback)
   {
      // Very simple implementation: could be optimized to minimize tree regeneration 
      // (local invalidation for example)//
      //
      regenerateAdminTree ();
   }

   // Y overrides ---------------------------------------------------
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   protected Iterator getTreesForResource(ManageableResource res, String profile, ArrayList pluginsSubset)
   {
      ArrayList result = new ArrayList ();
      

      for (int i = 0; i < pluginsSubset.size(); i++)
      {
         ConsolePlugin cp = (ConsolePlugin)pluginsSubset.get(i);
         TreeNode node = null;
         try
         {
            node = cp.getSubTreeForResource (this, profile, res);
         }
         catch (Throwable t) 
         { 
            t.printStackTrace(); 
         }
         
         if (node != null)
            result.add (node);
      }
      
      return result.iterator ();
   }
   
   protected ArrayList getPluginsSubsetForProfile (String profile)
   {
      ArrayList result = new ArrayList ();
      
      for (int i = 0; i < plugins.size(); i++)
      {
         ConsolePlugin cp = (ConsolePlugin)plugins.get(i);
         String [] set = cp.getSupportedProfiles ();
         if (java.util.Arrays.asList (set).contains (profile))
            result.add (cp);
      }
      
      return result;
   }
   
   protected HashSet findSubResources (TreeNode tree)
   {
      HashSet result = new HashSet ();
      
      // first add the tree node itself if it is an instance
      // of an ResourceTreeNode
      //
      if (tree instanceof ResourceTreeNode)
      {
         result.add (((ResourceTreeNode)tree).getResource ());
      }
      
      // then add local resources
      //
      ResourceTreeNode[] rns = tree.getNodeManagableResources ();
      if (rns != null && rns.length > 0)
      {      
         // Then travel to sub-nodes resources...
         //
         for (int i=0; i<rns.length; i++)
         {
            result.add (rns[i].getResource ());
            HashSet subResult = findSubResources (rns[i]);
            if (subResult != null && subResult.size () > 0)
               result.addAll (subResult);            
         }
      }
      
      // ..and to other sub-nodes (which are not resources)
      //
      TreeNode[] ns = tree.getSubNodes ();
      if (ns != null && ns.length > 0)
      {
         for (int i=0; i<ns.length; i++)
         {
            HashSet subResult = findSubResources (ns[i]);
            if (subResult != null && subResult.size () > 0)
               result.addAll (subResult);            
         }
      }
      
      return result;      
   }
   
   protected void bindProxyInJndi () throws Exception
   {
      InitialContext ctx = new InitialContext ();      
      Object proxy = RMIRemoteMBeanProxy.create (PluginManagerMBean.class, this.getServiceName (), this.getServer ());         
      org.jboss.naming.Util.rebind (ctx, this.jndiName, proxy);
   }
   
   protected void initNotificationReception () throws Exception
   {     
      ObjectName mbsDelegate = 
         new ObjectName ("JMImplementation:type=MBeanServerDelegate");
      
      NotificationFilter filter = new NotificationFilter ()
      {
         public boolean isNotificationEnabled (Notification n)
         {
            return ( n.getType().equals("JMX.mbean.registered") ||
                      n.getType().equals("JMX.mbean.unregistered") );
         }         
      };
      
      this.getServer().addNotificationListener(mbsDelegate, this, filter, null);
   }
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
