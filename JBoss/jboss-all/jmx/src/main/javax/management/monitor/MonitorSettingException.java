/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.monitor;

import javax.management.JMRuntimeException;

/**
 * Thrown by a monitor when a monitor setting becomes invalid.<p>
 *
 * ISSUE: Where and how is this used, if at all?
 *
 * @author <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>
 * @version $Revision: 1.1 $
 */
public class MonitorSettingException
   extends JMRuntimeException
{
   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * Construct a new MonitorSettingException with no message.
    */
   public MonitorSettingException()
   {
      super();
   }

   /**
    * Construct a new MonitorSettingException with the given message.
    *
    * @param message the error message.
    */
   public MonitorSettingException(String message)
   {
      super(message);
   }
}

