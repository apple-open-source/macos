/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.varia.scheduler;

import java.util.Date;

/**
 * This interface defines the manageable interface for a Scheduler Service
 * allowing the client to create a Schedulable instance which is then run
 * by this service at given times.
 *
 * @author <a href="mailto:andreas.schaefer@madplanet.com">Andreas Schaefer</a>
 * @version $Revision: 1.1 $
 */
public interface Schedulable
{
   /**
    * This method is called from the Scheduler Service
    *
    * @param pTimeOfCall              Date/Time of the scheduled call
    * @param pRemainingRepetitions    Number of the remaining repetitions which
    *                                 is -1 if there is an unlimited number of
    *                                 repetitions.
    */
   void perform(Date pTimeOfCall, long pRemainingRepetitions);
}
