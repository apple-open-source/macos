/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: AxisServiceMBean.java,v 1.5.4.1 2002/09/12 16:18:04 cgjung Exp $

package org.jboss.net.axis.server;

import org.jboss.net.axis.EngineConfigurationMBean;
import org.apache.axis.server.AxisServer;

/**
 * Mbean interface to the AxisService 
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @created 27. September 2001
 * @version $Revision: 1.5.4.1 $
 */

public interface AxisServiceMBean
   extends
      EngineConfigurationMBean,
      org.jboss.deployment.SubDeployer,
      org.jboss.system.ServiceMBean {
   /** returns the associated axis server */
   public AxisServer getAxisServer();
}
