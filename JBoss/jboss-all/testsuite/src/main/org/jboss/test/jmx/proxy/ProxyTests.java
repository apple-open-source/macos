package org.jboss.test.jmx.proxy;

import org.jboss.system.ServiceMBean;
import org.jboss.system.ServiceMBeanSupport;

/**
 * @author adrian@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class ProxyTests
   extends ServiceMBeanSupport 
   implements ProxyTestsMBean
{
   private TargetMBean proxy;

   public TargetMBean getProxy()
   {
      return proxy;
   }

   public void setProxy(TargetMBean proxy)
   {
      this.proxy = proxy;
   }
   
   public void startService()
      throws Exception
   {
      if (proxy == null)
         throw new RuntimeException("Proxy not started");
      
      if (proxy.getState() != ServiceMBean.STARTED)
        throw new RuntimeException("Proxy not started");
   }
}
