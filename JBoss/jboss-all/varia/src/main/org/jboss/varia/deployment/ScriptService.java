/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.varia.deployment;

import org.jboss.system.ServiceMBeanSupport;

/**
 * <description>
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>6 janv. 2003 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */
public interface ScriptService
   extends org.jboss.system.Service
{
   // Can be used by the script to 
   // define dependencies and its JMX name
   //
   public String[] dependsOn ();
   public String objectName ();
   public Class[] getInterfaces ();
   
   // Can be stored by the script to access its wrapper MBean
   //
   public void setCtx (ServiceMBeanSupport wrapper);
}
