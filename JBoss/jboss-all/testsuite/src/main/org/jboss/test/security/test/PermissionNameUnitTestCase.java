/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.security.test;

import java.io.FilePermission;
import java.net.URL;
import java.security.CodeSource;
import java.security.Permission;
import java.security.PermissionCollection;
import java.security.Policy;

/**
 * A JUnit TestCase for the PermissionNames class.
 *
 @author Scott.Stark@jboss.org
 @version $Revision: 1.3 $
 */
public class PermissionNameUnitTestCase
   extends junit.framework.TestCase
{
   public PermissionNameUnitTestCase(String name)
   {
      super(name);
   }

   /**
    * This method caueses errors, because newer junit this is a static.
    * and thus can not be overridden.
    * 
   protected void assertEquals(String msg, boolean expected, boolean actual)
   {
      assertTrue(msg, (expected == actual));
   }
   */
   
   /**
    * Test the order of PermissionNames
    */
   public void testOrdering()
   {
      String s0 = "starksm/Project1/Documents/readme.html";
      String s1 = "starksm/Project1/Documents/Folder1/readme.html";
      String s2 = "starksm/Project1/Documents";
      PermissionName n0 = new PermissionName(s0);
      PermissionName n1 = new PermissionName(s1);
      PermissionName n2 = new PermissionName(s2);

      assertTrue(n0.toString(), s0.equals(n0.toString()));
      assertTrue(n1.toString(), s1.equals(n1.toString()));
      assertEquals(s0, 4, n0.size());
      assertEquals(s1, 5, n1.size());
      assertEquals(s2, 3, n2.size());
      assertEquals("n0 < n1", true, (n0.compareTo(n1) < 0));
      assertEquals("n0 > n2", true, (n0.compareTo(n2) > 0));
   }
}
