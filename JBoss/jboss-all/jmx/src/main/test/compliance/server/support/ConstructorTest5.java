/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.server.support;

import javax.management.ObjectName;
import javax.management.MalformedObjectNameException;

/**
 * Support class which fails with an error in static initializer.
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.1 $
 *   
 */
public class ConstructorTest5
{

   static
   {
      try
      {
         new ObjectName(":");
      }
      catch (MalformedObjectNameException e)
      {
         throw new BabarError();
      }
   }
   
}
      



