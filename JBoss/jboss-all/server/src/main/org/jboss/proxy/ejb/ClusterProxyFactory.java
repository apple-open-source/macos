/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.proxy.ejb;

/**
 * Tag interface used to determine if a ProxyFactory is used for clustering purposes (to raise
 * warning if such a ProxyFactory is not found for a given bean when clustered=true)
 *
 * @see org.jboss.proxy.ejb.ProxyFactoryHA
 *
 * @author  <a href="mailto:sacha.labourey@jboss.org">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *   
 * <p><b>Revisions:</b>
 *
 * <p><b>16 August 2003 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li> 
 * </ul>
 */

public interface ClusterProxyFactory
{
}
