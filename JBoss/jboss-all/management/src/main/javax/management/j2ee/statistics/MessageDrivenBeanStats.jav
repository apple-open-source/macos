/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.j2ee.statistics;

/**
 * Represents a MessageDrivenBean's statistics as defined by JSR77.6.13
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.2 $
 */
public interface MessageDrivenBeanStats
      extends EJBStats
{
   /**
    * @return The number of messages received
    */
   public CountStatistic getMessageCount();
}
