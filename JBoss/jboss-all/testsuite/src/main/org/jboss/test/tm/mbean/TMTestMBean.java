package org.jboss.test.tm.mbean;

import org.jboss.system.ServiceMBean;
import org.jboss.test.tm.resource.Operation;

/**
 * @author adrian@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public interface TMTestMBean extends ServiceMBean
{
   void testOperations(String test, Operation[] ops) throws Exception;
}
