/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.manager.interfaces.impl;

import org.jboss.console.navtree.AppletBrowser;
import org.jboss.console.navtree.AppletTreeAction;
import org.jboss.console.navtree.TreeContext;

import javax.management.ObjectName;

/**
 * <description>
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.2 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>3 janv. 2003 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */
public class MBeanAction 
   implements AppletTreeAction
{
   protected ObjectName targetObjectName = null;
   protected String actionName = null;
   protected Object[] params = null;
   protected String[] signature = null;

   public MBeanAction () {}
   
   public MBeanAction (ObjectName pName,
                        String pActionName,
                        Object[] pParams,
                        String[] pSignature) 
   {
      this.targetObjectName = pName;
      this.actionName = pActionName;
      this.params = pParams;
      this.signature = pSignature;
   }   

   public void doAction(TreeContext tc, AppletBrowser applet)
   {
      try
      {
         tc.getRemoteMBeanInvoker ().invoke(targetObjectName, actionName, params, signature);
      }
      catch (Exception displayed)
      {
         displayed.printStackTrace();
      }
   }

}
