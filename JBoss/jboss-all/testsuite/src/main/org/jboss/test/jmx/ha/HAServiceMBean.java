package org.jboss.test.jmx.ha;

import java.util.Map;

import org.jboss.invocation.Invocation;
import org.jboss.system.ServiceMBean;

public interface HAServiceMBean
   extends ServiceMBean
{
   // For remoting
   Map getMethodMap();
   Object invoke(Invocation invocation) throws Exception;

}
