package org.jboss.test.jmx.proxy;

import org.jboss.system.ServiceMBean;

/**
 * @author adrian@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public interface ProxyTestsMBean
   extends ServiceMBean
{
   TargetMBean getProxy();
   void setProxy(TargetMBean proxy);
}
