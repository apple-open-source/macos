/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.plugins.helpers;

import org.jboss.console.manager.PluginManager;
import org.jboss.console.manager.interfaces.ConsolePlugin;
import org.jboss.console.manager.interfaces.ManageableResource;
import org.jboss.console.manager.interfaces.ResourceTreeNode;
import org.jboss.console.manager.interfaces.TreeAction;
import org.jboss.console.manager.interfaces.TreeNode;
import org.jboss.console.manager.interfaces.TreeNodeMenuEntry;
import org.jboss.console.manager.interfaces.impl.HttpLinkTreeAction;
import org.jboss.console.manager.interfaces.impl.MBeanResource;
import org.jboss.console.manager.interfaces.impl.SeparatorTreeNodeMenuEntry;
import org.jboss.console.manager.interfaces.impl.SimpleFolderResource;
import org.jboss.console.manager.interfaces.impl.SimpleResourceTreeNode;
import org.jboss.console.manager.interfaces.impl.SimpleTreeNode;
import org.jboss.console.manager.interfaces.impl.SimpleTreeNodeMenuEntryImpl;
import org.jboss.logging.Logger;
import org.jboss.mx.util.MBeanServerLocator;
import org.jboss.system.Registry;

import javax.management.MBeanServer;
import javax.management.MalformedObjectNameException;
import javax.management.ObjectInstance;
import javax.management.ObjectName;
import javax.management.Query;
import javax.management.QueryExp;
import javax.servlet.ServletConfig;
import java.util.HashMap;
import java.util.Set;

/**
 * <description>
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
public abstract class AbstractPluginWrapper
   implements PluginWrapper, ConsolePlugin
{
   // Constants -----------------------------------------------------
   
   public static final String OBJECT_NAME_PARAM = "ObjectName";
   public static final String FOLDER_NAME_PARAM = "FolderName";
   public static final String MBEAN_CLASS_PARAM = "MBeanClass";
   public static final String WRAPPER_CLASS_PARAM = "WrapperClass";
   public static final String SCRIPT_NAME_PARAM = "ScriptName";
   public static final String IS_ROOT_NODE_PARAM = "IsRootNode";

   // Attributes ----------------------------------------------------
   
   protected MBeanServer mbeanServer = null;   
   protected PluginManager pm = null;

   protected String pluginName = null;
   protected String pluginVersion = null;
   
   protected String objectName = null;
   protected String mbeanClass = null;
   protected String folderName = null;
     
   protected String rootContextName = null;
   
   protected Logger log = org.jboss.logging.Logger.getLogger(this.getClass());
   
   protected InternalResourceChecker checker = null;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
      
   public AbstractPluginWrapper () {}
   
   // Public --------------------------------------------------------
   
   // Z implementation ----------------------------------------------
   
   // PluginWrapper overrides ---------------------------------------
   
   public void init (ServletConfig servletConfig) throws Exception 
   {      
      findJBossMBeanServer ();      
      findPluginManager ();
      readConfigurationParameters (servletConfig);
            
      this.pm.registerPlugin(this);
   }

   public void destroy () 
   {
      this.pm.unregisterPlugin(this);
   }


   public void readConfigurationParameters (ServletConfig config)
   {      
      this.pluginName = config.getInitParameter("PluginName");
      this.pluginVersion = config.getInitParameter("PluginVersion");
      
      this.folderName = config.getInitParameter(FOLDER_NAME_PARAM);
      this.objectName = config.getInitParameter(OBJECT_NAME_PARAM);
      this.mbeanClass = config.getInitParameter(MBEAN_CLASS_PARAM);
      this.rootContextName = config.getInitParameter("ContextName");      

      String tmp = this.objectName;
      if (tmp != null && !"".equals(tmp))
      {
         // this kind of plugin is associated with a single MBean
         // which has a give JMX ObjectName
         //
         checker = new SingleMBeanChecker ();
      }

      tmp = this.folderName;
      if (tmp != null && !"".equals(tmp))
      {
         // this kind of plugins is associated with one of the static folder of the tree
         //
         checker = new SubFolderChecker ();
      }
      
      tmp = config.getInitParameter(IS_ROOT_NODE_PARAM);
      if (tmp != null && !"".equals(tmp) && "true".equalsIgnoreCase(tmp))
      {
         // this kind of plugins is associated with the root of the tree
         //
         checker = new RootTreeChecker ();
      }
      
      tmp = this.mbeanClass;
      if (tmp != null && !"".equals(tmp))
      {
         // this kind of plugins is associated with all MBean
         // that share a given interface
         //
         checker = new StandardMBeanChecker ();
      }

   }   
   
   // ConsolePlugin overrides ---------------------------------------
   
   public String getIdentifier()
   {
      if (this.pluginName != null)
      {
         return this.pluginName + " (Wrapped by ServletPluginHelper)";
      }
      else
      {
         return getPluginIdentifier();
      }
   }

   public String getVersion()
   {
      if (this.pluginVersion != null)
      {
         return this.pluginVersion;
      }
      else
      {
         return getPluginVersion ();
      }   
   }
   
   public String[] getSupportedProfiles()
   {
      return new String[] {ConsolePlugin.WEB_PROFILE};
   }

   public TreeNode getSubTreeForResource(
      PluginManager master,
      String profile,
      ManageableResource resource)
   {
      if (!ConsolePlugin.WEB_PROFILE.equalsIgnoreCase(profile))
      {
         return null;
      }
      else
      {
         if (isResourceToBeManaged (resource))
         {
            return getTreeForResource(
               profile,
               resource);
         }
         else
         {
            return null;
         }
      }
   }


   // Abstract Methods ---------------------------------------------

   protected boolean isResourceToBeManaged (ManageableResource resource)
   {
      if (checker == null)
      {
         return false;
      }
      else
      {
         return checker.isResourceToBeManaged(resource);
      }
   }
   
   protected abstract TreeNode getTreeForResource(
      String profile,
      ManageableResource resource);

   protected String getPluginIdentifier()
   {
      return "AbstractPluginWrapper (" + this.getClass() + ")";
   }

   protected String getPluginVersion()
   {
      return "unknown version";
   }

   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   
   protected void findJBossMBeanServer()
   {
      this.mbeanServer = MBeanServerLocator.locateJBoss();
   }
   
   protected void findPluginManager ()
   {
      this.pm = (PluginManager) Registry.lookup (PluginManager.PLUGIN_MANAGER_NAME);
   }

   protected MBeanServer getMBeanServer ()
   {
      return this.mbeanServer;
   }
   
   protected String fixUrl (String source)
   {
      if (source == null)
      {
         return null;
      }         
      else if (source.toLowerCase().startsWith("http://") || 
          source.toLowerCase().startsWith("https://"))
      {
         return source;
      }
      else if (source.startsWith("/"))
      {
         return source; // already absolute
      }
      else
      {
         return this.rootContextName + "/" + source;
      }
   }

   protected ObjectInstance[] getMBeansForClass(String scope, String className)
   {
      try
      {
         Set result = mbeanServer.queryMBeans(new ObjectName(scope), 
            Query.eq (Query.classattr(), Query.value(className)));
         
         return (ObjectInstance[])result.toArray(new ObjectInstance[result.size()]);
      }
      catch (MalformedObjectNameException e)
      {
         log.debug (e);
         return new ObjectInstance[0];
      }
         
   }

   protected ObjectInstance[] getMBeansForQuery(String scope, QueryExp query)
   {
      try
      {
         Set result = mbeanServer.queryMBeans(new ObjectName(scope), query);         
         return (ObjectInstance[])result.toArray(new ObjectInstance[result.size()]);
      }
      catch (MalformedObjectNameException e)
      {
         log.debug (e);
         return new ObjectInstance[0];
      }
         
   }

   protected SimpleTreeNode createTreeNode (String name,
                                            String description,
                                            String iconUrl,
                                            String defaultUrl,
                                            TreeNodeMenuEntry[] menuEntries,
                                            TreeNode[] subNodes,
                                            ResourceTreeNode[] subResNodes) throws Exception
   {
      TreeAction action = new HttpLinkTreeAction (fixUrl(defaultUrl));
      return new SimpleTreeNode (name, description, fixUrl(iconUrl), action, menuEntries, subNodes, subResNodes);
   }

   protected SimpleResourceTreeNode createResourceNode (String name,
                                            String description,
                                            String iconUrl,
                                            String defaultUrl,
                                            TreeNodeMenuEntry[] menuEntries,
                                            TreeNode[] subNodes,
                                            ResourceTreeNode[] subResNodes,
                                            String jmxObjectName,
                                            String jmxClassName) throws Exception
   {
      TreeAction action = new HttpLinkTreeAction (fixUrl(defaultUrl));
      ManageableResource res = new MBeanResource (new ObjectName(jmxObjectName), jmxClassName);
      return new SimpleResourceTreeNode (name, description, fixUrl(iconUrl), action, menuEntries, subNodes, subResNodes, res);
   }

   protected SimpleResourceTreeNode createResourceNode (String name,
                                            String description,
                                            String iconUrl,
                                            String defaultUrl,
                                            TreeNodeMenuEntry[] menuEntries,
                                            TreeNode[] subNodes,
                                            ResourceTreeNode[] subResNodes,
                                            ManageableResource resource) throws Exception
   {
      TreeAction action = new HttpLinkTreeAction (fixUrl(defaultUrl));
      return new SimpleResourceTreeNode (name, description, fixUrl(iconUrl), action, menuEntries, subNodes, subResNodes, resource);
   }

   protected TreeNodeMenuEntry[] createMenus (String[] content) throws Exception
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
               String text = content[i];
               TreeAction action = new HttpLinkTreeAction(fixUrl(content[i+1]));
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

   protected String encode (String source)
   {
      try
      {
         return java.net.URLEncoder.encode(source);
      }
      catch (Exception e)
      {
         return source;
      }
   }

   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
   public interface InternalResourceChecker
   {
      boolean isResourceToBeManaged (ManageableResource resource);
   }
   
   public class StandardMBeanChecker 
      implements InternalResourceChecker
   {
   
      protected Class targetClass = null;
      public HashMap knownAnswers = new HashMap ();
      
      public StandardMBeanChecker ()
      {    
         try
         {
            targetClass = Thread.currentThread().getContextClassLoader().loadClass(mbeanClass);
         }
         catch (Exception displayed)
         {
            displayed.printStackTrace();
         }
      }
   
      public boolean isResourceToBeManaged (ManageableResource resource)
      {
         if (resource instanceof MBeanResource)
         {
            MBeanResource mbr = (MBeanResource)resource;
            
            Boolean result = (Boolean)knownAnswers.get(mbr.getClassName ());
            if (result == null)
            {
               // find answer and cache it
               //
               try
               {
                  //System.out.println("CHECK: " + 
                  Class resourceClass = Thread.currentThread().getContextClassLoader().loadClass(mbr.getClassName ());
                  result = new Boolean (targetClass.isAssignableFrom(resourceClass));
                  //result = new Boolean (resourceClass.isAssignableFrom(targetClass));
               }
               catch (Exception e)
               {
                  result = Boolean.FALSE;
               }
               knownAnswers.put(mbr.getClassName(), result);
               
            }
            return result.booleanValue();         
         }
         else
            return false;
      }
   }
   

   public class RootTreeChecker 
      implements InternalResourceChecker
   {
   
      public boolean isResourceToBeManaged (ManageableResource resource)
      {
         if (resource == null)
            return false;
         else
            return resource.equals (pm.getBootstrapResource ());
      }
   }
   
   public class SingleMBeanChecker
      implements InternalResourceChecker
   {
      
      public boolean isResourceToBeManaged (ManageableResource resource)
      {
         if (objectName != null && resource instanceof MBeanResource)
         {
            MBeanResource mbr = (MBeanResource)resource;
            return objectName.equals(mbr.getObjectName().toString());
            
         }
         else
            return false;
      }
      
   }
   
   public class SubFolderChecker
      implements InternalResourceChecker
   {
      public boolean isResourceToBeManaged (ManageableResource resource)
      {
         if (resource == null || !(resource instanceof SimpleFolderResource))
         {         
            return false;
         }
         else
         {
            return folderName.equals(resource.getId());
         }        
      }
    }
   
}
