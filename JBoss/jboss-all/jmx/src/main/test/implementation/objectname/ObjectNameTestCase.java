/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.implementation.objectname;

import junit.framework.TestCase;

import javax.management.ObjectName;
import javax.management.MalformedObjectNameException;
import java.util.Hashtable;

/**
 * Tests wildcards in the hashtable property constructor <p>
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 */
public class ObjectNameTestCase
  extends TestCase
{
   // Attributes ----------------------------------------------------------------

   // Constructor ---------------------------------------------------------------

   /**
    * Construct the test
    */
   public ObjectNameTestCase(String s)
   {
      super(s);
   }

   // Tests --------------------------------------------------------------------

   /**
    * Test wild card in property this should work
    */
   public void testWildCardOneProperty()
      throws Exception
   {
      Hashtable ht = new Hashtable();
      ht.put("key", "value");
      ht.put("*", "*");
      ObjectName name = null;
      try
      {
         name = new ObjectName("domain", ht);
      }
      catch (MalformedObjectNameException e)
      {
         fail("Should accept *,* in properties");
      }
      if (name.isPattern() == false)
         fail("Name should be a pattern with properties: \n" + ht);
      if (name.isPropertyPattern() == false)
         fail("Name should be a property pattern with properties: \n" + ht);
      assertEquals("key=value,*", name.getCanonicalKeyPropertyListString());
   }

   /**
    * Test wild card in property this should work
    */
   public void testWildCardNoProperty()
      throws Exception
   {
      Hashtable ht = new Hashtable();
      ht.put("*", "*");
      ObjectName name = null;
      try
      {
         name = new ObjectName("domain", ht);
      }
      catch (MalformedObjectNameException e)
      {
         fail("Should accept *,* in properties");
      }
      if (name.isPattern() == false)
         fail("Name should be a pattern with properties: \n" + ht);
      if (name.isPropertyPattern() == false)
         fail("Name should be a property pattern with properties: \n" + ht);
      assertEquals("*", name.getCanonicalKeyPropertyListString());
   }

   /**
    * Test wild card in key only
    */
   public void testWildCardKeyOnly()
      throws Exception
   {
      Hashtable ht = new Hashtable();
      ht.put("*", "value");
      boolean caught = false;
      try
      {
         new ObjectName("domain", ht);
      }
      catch (MalformedObjectNameException e)
      {
         caught = true;
      }
      assertTrue(caught);
   }

   /**
    * Test wild card in value only
    */
   public void testWildCardValueOnly()
      throws Exception
   {
      Hashtable ht = new Hashtable();
      ht.put("key", "*");
      boolean caught = false;
      try
      {
         new ObjectName("domain", ht);
      }
      catch (MalformedObjectNameException e)
      {
         caught = true;
      }
      assertTrue(caught);
   }
}
