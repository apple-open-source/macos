/*
 * JBoss, the OpenSource J2EE server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.jmx.loading;

import org.jboss.system.ServiceMBean;

/** An mbean service that does nothing but references an external
 * class that is loaded from a jar refereced by the sar manifest
 *   
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2 $
 *
 */
public interface ExternalClassMBean extends ServiceMBean
{
}
