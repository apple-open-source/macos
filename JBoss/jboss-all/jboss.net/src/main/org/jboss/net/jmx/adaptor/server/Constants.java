/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.net.jmx.adaptor.server;

/**
 * Some Constants for the jmxadaptor package.
 *
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @created 1. October 2001
 * @version $Revision: 1.3 $
 */

public interface Constants extends org.jboss.net.Constants
{
    String DOMAIN = "jboss.net";
    String NAME = "JMXConnector";
    String TYPE = "service";
    String DEFAULT_AXIS_SERVICE_NAME="jboss.net=AxisService";
    String JMX_INSTALL_DESCRIPTOR="META-INF/install-jmx.xml";
}
