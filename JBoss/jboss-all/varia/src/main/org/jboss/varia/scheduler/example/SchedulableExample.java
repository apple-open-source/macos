/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.varia.scheduler.example;

import java.util.Date;

import org.jboss.logging.Logger;

import org.jboss.varia.scheduler.Schedulable;

/**
 * A simple Schedulable implementaion that logs when an event occurs.
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @author Cameron (camtabor)
 * @version $Revision: 1.1 $
 */
public class SchedulableExample
   implements Schedulable
{
   /** Class logger. */
   private static final Logger log = Logger.getLogger(SchedulableExample.class);
      
   private String mName;
   private int mValue;
   
   public SchedulableExample(String pName, int pValue)
   {
      mName = pName;
      mValue = pValue;
   }

   /**
    * Just log the call
    **/
   public void perform(Date pTimeOfCall, long pRemainingRepetitions)
   {
      log.info("Schedulable Examples is called at: " + pTimeOfCall +
               ", remaining repetitions: " + pRemainingRepetitions +
               ", test, name: " + mName + ", value: " + mValue);
   }
}
