/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.tomcat.tc5.session;

/** The MBean-interface for the ClusterManager
 *
 @see org.jboss.ha.httpsession.server.ClusteredHTTPSessionService

 @author Thomas Peuss <jboss@peuss.de>
 @version $Revision: 1.1.2.1 $
 */
public interface JBossManagerMBean
{
   public Integer getLocalActiveSessionCount();
}
