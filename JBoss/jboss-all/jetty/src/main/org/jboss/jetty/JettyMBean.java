/*
 * jBoss, the OpenSource EJB server
 *
 * Distributable under GPL license.
 * See terms of license at gnu.org.
 */

// $Id$

package org.jboss.jetty;

import javax.management.InstanceNotFoundException;
import javax.management.MBeanException;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import org.mortbay.jetty.jmx.ServerMBean;

public class JettyMBean
  extends ServerMBean
{
  public static final String JBOSS_DOMAIN = "jboss.web";

  static
  {
    setDefaultDomain (JBOSS_DOMAIN);
  }

  public JettyMBean(Jetty jetty)
    throws MBeanException, InstanceNotFoundException
  {
    super(jetty);
  }
}
