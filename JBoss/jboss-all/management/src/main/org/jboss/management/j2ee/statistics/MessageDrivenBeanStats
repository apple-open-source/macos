/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.statistics;

import javax.management.j2ee.statistics.RangeStatistic;
import javax.management.j2ee.statistics.MessageDrivenBeanStats;
import javax.management.j2ee.statistics.CountStatistic;

import org.jboss.management.j2ee.statistics.RangeStatisticImpl;

/** The JSR77.6.13 MessageDrivenBeanStats implementation
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class MessageDrivenBeanStatsImpl extends EJBStatsImpl
   implements MessageDrivenBeanStats
{
   private CountStatisticImpl messageCount;

   public MessageDrivenBeanStatsImpl()
   {
      messageCount = new CountStatisticImpl("MessageCount", "1",
         "The count of messages received");
      addStatistic("MessageCount", messageCount);
   }

// Begin javax.management.j2ee.statistics.MessageDrivenBeanStats interface methods

   public CountStatistic getMessageCount()
   {
      return messageCount;
   }

// End javax.management.j2ee.statistics.MessageDrivenBeanStats interface methods
}
