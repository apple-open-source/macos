/*
 * JBossMQ, the OpenSource JMS implementation
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mq.server;

import java.lang.Comparable;

/**
 * Much like the <code>java.util.TimerTask</code> class, but simplified.
 */
abstract class SimpleTimerTask
   implements Runnable, Comparable
{
   protected long scheduled = 0;
   public final int compareTo(Object o)
   {
      long t = ((SimpleTimerTask)o).scheduled;
      if (t < scheduled) return 1;
      if (t > scheduled) return -1;
      return 0;
   }
}
