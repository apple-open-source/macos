/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.logging.layout;

import org.apache.log4j.MDC;
import org.apache.log4j.helpers.FormattingInfo;
import org.apache.log4j.helpers.PatternConverter;
import org.apache.log4j.spi.LoggingEvent;

/** A PatternConverter that uses the current thread MDC rather than the
 * LoggingEvent MDC value.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class ThreadMDCConverter extends PatternConverter
{
   private String key;
   /** Creates a new instance of ThreadMDCPatternConverter */
   public ThreadMDCConverter(FormattingInfo formattingInfo, String key)
   {
      super(formattingInfo);
      this.key = key;
   }

   protected String convert(LoggingEvent loggingEvent)
   {
      Object val = MDC.get(key);
      String strVal = null;
      if( val != null )
         strVal = val.toString();
      return strVal;
   }

}
