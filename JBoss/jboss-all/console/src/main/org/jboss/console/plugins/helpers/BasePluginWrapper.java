/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.plugins.helpers;

import bsh.Interpreter;
import org.jboss.console.manager.PluginManager;
import org.jboss.console.manager.interfaces.ManageableResource;
import org.jboss.console.manager.interfaces.ResourceTreeNode;
import org.jboss.console.manager.interfaces.TreeNode;
import org.jboss.console.manager.interfaces.TreeNodeMenuEntry;
import org.jboss.logging.Logger;

import javax.management.MBeanServer;
import javax.management.ObjectInstance;
import javax.servlet.ServletConfig;
import java.lang.reflect.UndeclaredThrowableException;

/**
 * <description>
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.2.2.2 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>23 dec 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */
public class BasePluginWrapper
   extends AbstractPluginWrapper
{
   
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------
   
   protected Interpreter interpreter = null;

   protected String pluginName = null;
   protected String pluginVersion = null;
   
   protected String scriptName = null;
   
   protected String scriptContent = null;
   
   protected ScriptPlugin script = null;
   protected PluginContext pluginCtx = null;
   
   // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
      
   public BasePluginWrapper () { super (); }
   
   // Public --------------------------------------------------------
   
   // Z implementation ----------------------------------------------
   
   // PluginWrapper overrides ---------------------------------------
   
   public void init (ServletConfig servletConfig) throws Exception 
   {
      super.init (servletConfig);
      
      loadScript (this.scriptName);      
      pluginCtx = new SimplePluginContext ();

   }

   public void readConfigurationParameters (ServletConfig config)
   {      
      /*
      try
      {
         this.rootContextName = config.getServletContext().getServletContextName();
         log.info ("XXXXXXXXXXXXXXXXXX* * " + rootContextName);
         log.info ("XXXXXXXXXXXXXXXXXX* * " + rootContextName);         
      }
      catch (Exception ignored) {}
      getRealPath("/");
      */
      
      super.readConfigurationParameters(config);
      
      this.scriptName = config.getInitParameter("ScriptName");
   }   
   
   // ConsolePlugin overrides ---------------------------------------
   
   protected String getPluginIdentifier()
   {
      try
      {
         return script.getName (pluginCtx);
      }
      catch (UndeclaredThrowableException ute)
      {
         return "ServletPluginHelper Wrapping script '" + this.scriptName + "'";
      }
   }

   protected String getPluginVersion()
   {
      try
      {
         System.out.println ("Version : " + script.getVersion (pluginCtx));
         return script.getVersion (pluginCtx);
      }
      catch (UndeclaredThrowableException ute)
      {
         return "unknown version";
      }
   }
   
   protected TreeNode getTreeForResource(
      String profile,
      ManageableResource resource)
   {
      try
      {
         TreeNode result = script.getTreeForResource (resource, pluginCtx);
         // result = fixUrls (result); // no really necessary now!
         return result;
      }
      catch (UndeclaredThrowableException ute)
      {
         ute.printStackTrace(); // TODO CHANGE TO LOG.DEBUG!!!
         return null; // we decide for the plugin: we don't provide content
      }
   }

   protected boolean isResourceToBeManaged (ManageableResource resource)
   {
      if (checker != null)
         return super.isResourceToBeManaged(resource);
      else
      {
         try
         {
            return isResourceToBeManaged_Script (pm, resource);
         }
         catch (UndeclaredThrowableException ute)
         {
            ute.printStackTrace();
            return false; // we decide for the plugin (not implemented by it)
         }
      }
   }
   
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   protected boolean isResourceToBeManaged_Script (PluginManager master, 
                                              ManageableResource resource)
                                              throws UndeclaredThrowableException
   {
      return script.isResourceToBeManaged(resource, pluginCtx);
   }
        
   protected void loadScript (String scriptName) throws Exception
   {
      java.net.URL url = Thread.currentThread().getContextClassLoader().getResource(scriptName);
      interpreter = new Interpreter (); 
      //System.out.println(Thread.currentThread().getContextClassLoader());
      interpreter.setClassLoader(Thread.currentThread().getContextClassLoader());
      //interpreter.eval (new java.io.InputStreamReader (url.openStream()), new NameSpace (this.rootContextName), this.rootContextName);
      interpreter.eval (new java.io.InputStreamReader (url.openStream()));
      //interpreter.source (url.getFile(), new bsh.NameSpace(this.rootContextName));
      
      script = (ScriptPlugin)interpreter.getInterface(ScriptPlugin.class);
      
   }   

   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
   public class SimplePluginContext implements PluginContext
   {
      public String localizeUrl (String source)
      {
         return fixUrl (source);         
      }
      
      public MBeanServer getLocalMBeanServer()
      {
         return mbeanServer;
      }

      public ObjectInstance[] getMBeansForClass(String scope, String className)
      {
         return BasePluginWrapper.this.getMBeansForClass (scope, className);            
      }

      public Logger getLogger()
      {
         return log;
      }

      public TreeNode createTreeNode (String name,
                                               String description,
                                               String iconUrl,
                                               String defaultUrl,
                                               TreeNodeMenuEntry[] menuEntries,
                                               TreeNode[] subNodes,
                                               ResourceTreeNode[] subResNodes) throws Exception
      {
         return BasePluginWrapper.this.createTreeNode (name, description, iconUrl, defaultUrl, menuEntries, subNodes, subResNodes);            
      }
   
      public ResourceTreeNode createResourceNode (String name,
                                               String description,
                                               String iconUrl,
                                               String defaultUrl,
                                               TreeNodeMenuEntry[] menuEntries,
                                               TreeNode[] subNodes,
                                               ResourceTreeNode[] subResNodes,
                                               String jmxObjectName,
                                               String jmxClassName) throws Exception
      {
         return BasePluginWrapper.this.createResourceNode (name,
                                               description,
                                               iconUrl,
                                               defaultUrl,
                                               menuEntries,
                                               subNodes,
                                               subResNodes,
                                               jmxObjectName,
                                               jmxClassName);
      }
   
      public ResourceTreeNode createResourceNode (String name,
                                               String description,
                                               String iconUrl,
                                               String defaultUrl,
                                               TreeNodeMenuEntry[] menuEntries,
                                               TreeNode[] subNodes,
                                               ResourceTreeNode[] subResNodes,
                                               ManageableResource resource) throws Exception
      {
         return BasePluginWrapper.this.createResourceNode (name,
                                               description,
                                               iconUrl,
                                               defaultUrl,
                                               menuEntries,
                                               subNodes,
                                               subResNodes,
                                               resource);
      }
   
      public TreeNodeMenuEntry[] createMenus (String[] content) throws Exception
      {
         return BasePluginWrapper.this.createMenus (content);
      }
      
      public String encode (String source)
      {
         return BasePluginWrapper.this.encode (source);
      }


   }

}
