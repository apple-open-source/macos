/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: AxisServiceMBean.java,v 1.5.4.2 2003/11/06 15:36:05 cgjung Exp $

package org.jboss.net.axis.server;

import org.apache.axis.server.AxisServer;
import org.apache.axis.EngineConfiguration;

/**
 * Mbean interface to the AxisService 
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @created 27. September 2001
 * @version $Revision: 1.5.4.2 $
 */

public interface AxisServiceMBean
   extends
      org.jboss.deployment.SubDeployer,
      org.jboss.system.ServiceMBean {
   /** returns the associated axis server */
   public AxisServer getAxisServer();

   /** return the associated client configuration */
   public EngineConfiguration getClientEngineConfiguration();

   /** return the associated server configuration */
   public EngineConfiguration getServerEngineConfiguration();
}
