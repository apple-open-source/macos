package org.jboss.test.classloader.interrupt;

import org.apache.log4j.Logger;

/** A thread subclass that loads a class while its interrupted flag is set.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2.2.1 $
 */
public class TestThread extends Thread
{
   private static Logger log = Logger.getLogger(TestThread.class);
   private Object listener;
   Throwable ex;

   /** Creates a new instance of TestThread */
   public TestThread(Object listener)
   {
      super("org.jboss.test.classloader.interrupt.TestThread");
      this.listener = listener;
   }

	public void run()
   {
      // Set our interrupted flag
      log.debug("Setting interrupt flag");
      this.interrupt();
      try
      {
         // An explict reference to TestClass will invoke loadClassInternal
         log.debug("Creating TestClass");
         TestClass tc = new TestClass();
         log.debug("TestClass instance = "+tc);
         if( this.isInterrupted() == false )
            ex = new IllegalStateException("Interrupted state not restore after loadClassInternal");
      }
      catch(Throwable e)
      {
         this.ex = e;
         log.error("Failure creating TestClass", e);
      }
	}
}
