package org.jboss.test.classloader.scoping.singleton;

import org.jboss.system.ServiceMBean;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public interface TestServiceMBean extends ServiceMBean
{
   public boolean checkVersion(String version);
}
