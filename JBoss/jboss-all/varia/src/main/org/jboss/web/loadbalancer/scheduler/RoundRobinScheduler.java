/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.loadbalancer.scheduler;

import java.net.URL;

/**
 * A round robin scheduler.
 * @jmx:mbean name="jboss.web.loadbalancer: service=RoundRobinScheduler"
 *            extends="org.jboss.web.loadbalancer.scheduler.SchedulerBaseMBean"
 *
 * @author Thomas Peuss <jboss@peuss.de>
 * @version $Revision: 1.8.2.1 $
 */
public class RoundRobinScheduler
    extends SchedulerBase implements RoundRobinSchedulerMBean {
  private int index = 0;
  public RoundRobinScheduler() {
  }

  protected URL getNextHost() {
    URL host = null;
    try {
      host = (URL) hostsUp.get(index++);
    }
    catch (IndexOutOfBoundsException iobex) {
      index = 0;
    }
    return host;
  }
}
