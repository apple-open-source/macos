/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.manager.interfaces;

import org.jboss.console.manager.PluginManager;

/**
 * Interface that all management plugins must implement
 *
 * @see org.jboss.console.manager.PluginManager
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>14 decembre 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public interface ConsolePlugin
   extends java.io.Serializable
{
   public final static String WEB_PROFILE = "WEB";
   
   public final static String[] PLUGIN_PROFILES = {WEB_PROFILE};
   
   /**
    * Define in which console framework this plugin can be plugged (web, eclipse, etc.)
    */
   String[] getSupportedProfiles ();
   
   public TreeNode getSubTreeForResource (PluginManager master, String profile, ManageableResource resource);
   
   
   /*
    * Plugin identifier: used by the PluginManager to list all activated plugins
    */
   public String getIdentifier();

   /*
    * Plugin version: used by the PluginManager to list all activated plugins
    */
   public String getVersion();
   
}
