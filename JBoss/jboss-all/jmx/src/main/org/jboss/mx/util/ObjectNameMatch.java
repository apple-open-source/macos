package org.jboss.mx.util;

import java.util.Hashtable;
import java.util.Iterator;
import javax.management.ObjectName;
import gnu.regexp.RE;
import gnu.regexp.REException;

/** JMX ObjectName comparision utility methods
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class ObjectNameMatch
{
   /** Compare two ObjectNames to see if the match via equality or as
    * a pattern.
    * @param n0 An ObjectName or pattern
    * @param n1 An ObjectName or pattern
    * @return true if n0 and n1 match, false otherwise
    */
   public static boolean match(ObjectName n0, ObjectName n1)
   {
      boolean match = n0.equals(n1);
      if( match == true )
         return true;

      // First compare the domains
      String d0 = n0.getDomain();
      String d1 = n1.getDomain();
      int star0 = d0.indexOf('*');
      int star1 = d1.indexOf('*');

      if( star0 >= 0 )
      {
         if( star1 >= 0 )
         {
            match = d0.equals(d1);
         }
         else
         {
            try
            {
               RE domainRE = new RE(d0);
               match = domainRE.isMatch(d1);
            }
            catch (REException e)
            {
            }
         }
      }
      else if( star1 >= 0 )
      {
         if( star0 >= 0 )
         {
            match = d0.equals(d1);
         }
         else
         {
            try
            {
               RE domainRE = new RE(d1);
               match = domainRE.isMatch(d0);
            }
            catch (REException e)
            {
            }
         }
      }
      else
      {
         match = d0.equals(d1);
      }

      if( match == false )
         return false;

      // Next compare properties
      if( n0.isPropertyPattern() )
      {
         Hashtable props0 = n0.getKeyPropertyList();
         Hashtable props1 = n1.getKeyPropertyList();
         Iterator iter = props0.keySet().iterator();
         while( match == true && iter.hasNext() )
         {
            String key = (String) iter.next();
            String value = (String) props0.get(key);
            match &= value.equals(props1.get(key));
         }
      }
      else if( n1.isPropertyPattern() )
      {
         Hashtable props0 = n0.getKeyPropertyList();
         Hashtable props1 = n1.getKeyPropertyList();
         Iterator iter = props1.keySet().iterator();
         while( iter.hasNext() )
         {
            String key = (String) iter.next();
            String value = (String) props1.get(key);
            match &= value.equals(props0.get(key));
         }
      }

      return match;
   }

}
