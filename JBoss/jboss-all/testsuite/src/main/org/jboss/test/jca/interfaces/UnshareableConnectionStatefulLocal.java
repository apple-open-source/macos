package org.jboss.test.jca.interfaces;

import javax.ejb.EJBLocalObject;

public interface UnshareableConnectionStatefulLocal
   extends EJBLocalObject
{
   void runTestPart1();
   void runTestPart2();
}
