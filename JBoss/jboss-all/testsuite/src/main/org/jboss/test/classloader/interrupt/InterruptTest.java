
package org.jboss.test.classloader.interrupt;

import org.jboss.system.ServiceMBeanSupport;

/** A simple service that creates a thread that tries to load a class
 while its interrupted flag is set. This is based on the example
 submitted with bug 563988 submitted by Harald Gliebe.

@author Scott.Stark@jboss.org
@version $Revision: 1.2.2.1 $
 */
public class InterruptTest extends ServiceMBeanSupport
   implements InterruptTestMBean
{

	protected void startService() throws Exception
   {
		log.debug("Starting the TestThread");
		TestThread thread = new TestThread(this);
      thread.start();
      try
      {
         thread.join();
      }
      catch(InterruptedException e)
      {
         log.debug("Was interrupted during join", e);
      }
      log.debug("TestThread complete, ex="+thread.ex);
      if( thread.ex != null )
         throw new ExceptionInInitializerError(thread.ex);
	}
}
