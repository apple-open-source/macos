/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: AdaptorMBean.java,v 1.2.4.1 2002/09/12 16:18:05 cgjung Exp $

package org.jboss.net.jmx.adaptor.server;

import javax.management.MBeanServer;

import org.jboss.system.ServiceMBean;

/**
 * just for compliance purposes, we have to
 * provide an empty mbean interface
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @created October 2, 2001
 * @version $Revision: 1.2.4.1 $
 */

public interface AdaptorMBean extends ServiceMBean, MBeanServer {

}