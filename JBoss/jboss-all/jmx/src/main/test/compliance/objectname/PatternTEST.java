/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package test.compliance.objectname;

import junit.framework.TestCase;

import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;

public class PatternTEST extends TestCase
{
   public PatternTEST(String s)
   {
      super(s);
   }

   public void testBasicDomainPattern()
   {
      String nameArg = "*:key1=val1,key2=val2";
      ObjectName name = constructSafely(nameArg);
      assertTrue("isPattern should be true", name.isPattern());
      assertEquals("toString should be: '" + nameArg + "'", nameArg, name.toString());
      assertTrue("isPropertyPattern should be false", !name.isPropertyPattern());
      assertEquals("*", name.getDomain());
   }

   public void testBasicDomainPatternExtra()
   {
      String nameArg = "**:key1=val1,key2=val2";
      ObjectName name = constructSafely(nameArg);
      assertTrue("isPattern should be true", name.isPattern());
      assertEquals("toString should be: '" + nameArg + "'", nameArg, name.toString());
      assertTrue("isPropertyPattern should be false", !name.isPropertyPattern());
      assertEquals("**", name.getDomain());
   }

   public void testPartialDomainPattern()
   {
      String nameArg = "*domain:key1=val1,key2=val2";
      ObjectName name = constructSafely(nameArg);
      assertTrue("isPattern should be true", name.isPattern());
      assertEquals("toString should be: '" + nameArg + "'", nameArg, name.toString());
      assertTrue("isPropertyPattern should be false", !name.isPropertyPattern());
      assertEquals("*domain", name.getDomain());
   }

   public void testHarderPartialDomainPattern()
   {
      String nameArg = "d*n:key1=val1,key2=val2";
      ObjectName name = constructSafely(nameArg);
      assertTrue("isPattern should be true", name.isPattern());
      assertEquals("toString should be: '" + nameArg + "'", nameArg, name.toString());
      assertTrue("isPropertyPattern should be false", !name.isPropertyPattern());
      assertEquals("d*n", name.getDomain());
   }

   public void testHarderPartialDomainPatternExtra()
   {
      String nameArg = "d**n:key1=val1,key2=val2";
      ObjectName name = constructSafely(nameArg);
      assertTrue("isPattern should be true", name.isPattern());
      assertEquals("toString should be: '" + nameArg + "'", nameArg, name.toString());
      assertTrue("isPropertyPattern should be false", !name.isPropertyPattern());
      assertEquals("d**n", name.getDomain());
   }

   public void testPositionalDomainPattern()
   {
      String nameArg = "do??in:key1=val1,key2=val2";
      ObjectName name = constructSafely(nameArg);
      assertTrue("isPattern should be true", name.isPattern());
      assertEquals("toString should be: '" + nameArg + "'", nameArg, name.toString());
      assertTrue("isPropertyPattern should be false", !name.isPropertyPattern());
      assertEquals("do??in", name.getDomain());
   }

   public void testPatternOnly()
   {
      String nameArg = "*:*";
      ObjectName name = constructSafely(nameArg);
      assertTrue("isPattern should be true", name.isPattern());
      assertTrue("isPropertyPattern should be true", name.isPropertyPattern());
      // The RI incorrectly (IMHO) removes the * from propertyPatterns
      assertEquals("FAILS IN RI", nameArg, name.getCanonicalName());
   }

   public void testKeyPatternOnly()
   {
      String nameArg = "domain:*";
      ObjectName name = constructSafely(nameArg);
      assertTrue("isPattern should be true", name.isPattern());
      assertTrue("isPropertyPattern should be true", name.isPropertyPattern());
      // The RI incorrectly (IMHO) removes the * from propertyPatterns
      assertEquals("FAILS IN RI", nameArg, name.getCanonicalName());
      assertTrue("key properties hash should be zero size", 0 == name.getKeyPropertyList().size());
   }

   public void testPartialKeyPattern()
   {
      String nameArg = "domain:key2=val2,*,key1=val1";
      ObjectName name = constructSafely(nameArg);
      assertTrue("isPattern should be true", name.isPattern());
      assertTrue("isPropertyPattern should be true", name.isPropertyPattern());
      // The RI incorrectly (IMHO) removes the * from propertyPatterns
      assertEquals("FAILS IN RI", "domain:key1=val1,key2=val2,*", name.getCanonicalName());
      assertTrue("key properties hash should only have 2 elements", 2 == name.getKeyPropertyList().size());
   }

   public void testEquality_a()
   {
      ObjectName pat1 = constructSafely("domain:*,key=value");
      ObjectName pat2 = constructSafely("domain:key=value,*");
      assertEquals(pat1, pat2);
   }

   public void testEquality_b()
   {
      ObjectName pat1 = constructSafely("do**main:key=value,*");
      ObjectName pat2 = constructSafely("do*main:key=value,*");
      assertTrue(".equals() should return false", !pat1.equals(pat2));
   }

/* FIXME THS - this test fails when run against the RI!
   public void testEquality_c()
   {
      ObjectName conc = constructSafely("domain:key=value");
      ObjectName pat = constructSafely("domain:key=value,*");
      assertEquals("toString() should match", conc.toString(), pat.toString());
      assertTrue("equals() should be false", !conc.equals(pat));
   }
*/

   private ObjectName constructSafely(String nameArg)
   {
      ObjectName name = null;
      try
      {
         name = new ObjectName(nameArg);
      }
      catch (MalformedObjectNameException e)
      {
         fail("spurious MalformedObjectNameException on ('" + nameArg + "')");
      }

      return name;
   }
}
