/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.logging.layout;

import org.apache.log4j.helpers.PatternParser;

/** A subclass of the log4j PatternLayout that add the following conversion
characters:

   <p>
   <table border="1" CELLPADDING="8">
   <th>Conversion Character</th>
   <th>Effect</th>

   <tr>
     <td align=center><b>z</b></td>
     <td>Used to output current thread NDC value. This can be used to obtain
      an NDC to augment any NDC associated with the LoggingEvent. This might
      be necessary if the LoggingEvent has been serialized between VMs.
     </td>
   </tr>
   <tr>
     <td align=center><b>Z</b></td>
     <td>Used to output current thread MDC value. This can be used to obtain
      an MDC to augment any MDC associated with the LoggingEvent. This might
      be necessary if the LoggingEvent has been serialized between VMs.
      The Z conversion character must be followed by the key for the map placed
      between braces, as in %Z{theKey} where theKey is the key.
      The value in the MDC corresponding to the key will be output.
     </td>
   </tr>

 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.4.1 $
 */
public class PatternLayout extends org.apache.log4j.PatternLayout
{

  protected PatternParser createPatternParser(String pattern)
  {
    return new PatternParserEx(pattern);
  }

}
