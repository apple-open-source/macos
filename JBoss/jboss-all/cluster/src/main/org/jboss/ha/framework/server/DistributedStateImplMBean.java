/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.server;

/**
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.3 $
 *   
 * <p><b>Revisions:</b>
 *
 * <p><b>29. décembre 2001 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li> 
 * </ul>
 */
public interface DistributedStateImplMBean
   extends org.jboss.ha.framework.interfaces.DistributedState
{
   String listContent () throws Exception;
   String listXmlContent () throws Exception;
}
