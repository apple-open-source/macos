/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.compliance.objectname;

import org.jboss.test.jbossmx.compliance.TestCase;

import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import java.util.Hashtable;

public class CanonicalTestCase
   extends TestCase
{
   public static final String EXPECTED_NAME = "domain:a=a,b=b,c=c,d=d,e=e";
   public static final String[] KVP = {"a", "b", "c", "d", "e"};

   public CanonicalTestCase(String s)
   {
      super(s);
   }

   public void testBasicCanonical()
   {
      try
      {
         ObjectName name = new ObjectName("domain:e=e,b=b,d=d,c=c,a=a");
         assertEquals(EXPECTED_NAME, name.getCanonicalName());
      }
      catch (MalformedObjectNameException e)
      {
         fail("spurious MalformedObjectNameException");
      }
   }

   public void testHashtableCanonical()
   {
      try
      {
         Hashtable h = new Hashtable();
         for (int i = 0; i < KVP.length; i++)
         {
            h.put(KVP[i], KVP[i]);
         }
         ObjectName name = new ObjectName("domain", h);
         assertEquals(EXPECTED_NAME, name.getCanonicalName());
      }
      catch (MalformedObjectNameException e)
      {
         fail("spurious MalformedObjectNameException");
      }
   }

   public void testSingleKVP()
   {
      try
      {
         ObjectName name = new ObjectName("domain", "a", "a");
         assertEquals("domain:a=a", name.getCanonicalName());
      }
      catch (MalformedObjectNameException e)
      {
         fail("spurious MalformedObjectNameException");
      }
   }
}
