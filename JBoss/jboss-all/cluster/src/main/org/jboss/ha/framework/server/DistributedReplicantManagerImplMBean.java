/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.server;

/**
 * Used as a static MBean interface for the DRM service
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.2 $
 *   
 * <p><b>Revisions:</b>
 *
 * <p><b>12 janvier 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li> 
 * </ul>
 */

public interface DistributedReplicantManagerImplMBean
   extends org.jboss.ha.framework.interfaces.DistributedReplicantManager
{
   String listContent () throws Exception;
   String listXmlContent () throws Exception;
}
