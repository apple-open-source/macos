/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.tm.mbean;

import org.jboss.system.ServiceMBeanSupport;
import org.jboss.test.tm.resource.Operation;
import org.jboss.util.NestedException;

/** 
 * Server Side TM test.
 *
 * @author adrian@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class TMTest
   extends ServiceMBeanSupport
   implements TMTestMBean
{
   public void testOperations(String test, Operation[] ops) throws Exception
   {
      log.info("Starting test " + test);
      Operation.start(log);
      int i = 0;
      try
      {
         for (;i < ops.length; ++i)
            ops[i].perform();
      }
      catch (Exception e)
      {
         throw new NestedException(test + " operation " + i, e);
      }
      finally
      {
         log.info("Finished test " + test);
         Operation.end();
      }
   }
}
