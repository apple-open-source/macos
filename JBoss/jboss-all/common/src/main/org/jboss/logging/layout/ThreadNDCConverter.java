/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.logging.layout;

import org.apache.log4j.NDC;
import org.apache.log4j.helpers.FormattingInfo;
import org.apache.log4j.helpers.PatternConverter;
import org.apache.log4j.spi.LoggingEvent;

/** A PatternConverter that uses the current thread NDC rather than the
 * LoggingEvent NDC value.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class ThreadNDCConverter extends PatternConverter
{
   /** Creates a new instance of ThreadMDCPatternConverter */
   public ThreadNDCConverter(FormattingInfo formattingInfo)
   {
      super(formattingInfo);
   }

   protected String convert(LoggingEvent loggingEvent)
   {
      Object val = NDC.get();
      String strVal = null;
      if( val != null )
         strVal = val.toString();
      return strVal;
   }

}
