package org.jboss.test.util.scheduler;

import java.util.Date;

import org.jboss.logging.Logger;
import org.jboss.varia.scheduler.Schedulable;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class TestSchedulable
   implements Schedulable
{
   private static final Logger log = Logger.getLogger(TestSchedulable.class);

   private String name;
   private int data;

   public TestSchedulable(String name, int data)
   {
      this.name = name;
      this.data = data;
   }

   /**
    * Just log the call
    */
   public void perform(Date time, long remaining)
   {
      log.info("Schedulable Examples is called at: " + time +
         ", remaining repetitions: " + remaining +
         ", name: " + name + ", data: " + data);
   }
}