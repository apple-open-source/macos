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
 * A JUnit TestCase for the NamespacePermissions and NamespacePermission
 * classes.
 *
@author Scott.Stark@jboss.org
@version $Revision: 1.3 $
*/
public class NamespacePermissionsUnitTestCase
   extends junit.framework.TestCase
{
   PermissionCollection pc;

   public NamespacePermissionsUnitTestCase(String name)
   {
      super(name);
   }

   protected void setUp() throws Exception
   {
      pc = new NamespacePermissionCollection();
      NamespacePermission p = new NamespacePermission("starksm/Project1", "r---");
      pc.add(p);
      p = new NamespacePermission("starksm/Project1/Documents/readme.html", "rw--");
      pc.add(p);
      p = new NamespacePermission("starksm/Project1/Documents/Public", "rw--");
      pc.add(p);
      p = new NamespacePermission("starksm/Project1/Documents/Public/Private", "----");
      pc.add(p);
      p = new NamespacePermission("Project1/Documents/Public", "r---");
      pc.add(p);
      p = new NamespacePermission("Project1/Documents/Public/starksm", "----");
      pc.add(p);
   }
   protected void tearDown()
   {
      pc = null;
   }

   /**
    * This method caueses errors, because newer junit this is a static.
    * and thus can not be overridden.
    * 
   protected void assertEquals(String msg, boolean expected, boolean actual)
   {
      assert(msg, (expected == actual));
   }
   */
   
   /**
    * Test the NamespacePermissionCollection implies method for various
    * permission that should be implied by the setup PermissionCollection.
    */
   public void testImplied()
   {
      NamespacePermission p = new NamespacePermission("Project1/Documents/Public/view1.jpg", "r---");
      boolean implied = pc.implies(p);
      assertEquals(p.toString(), true, implied);
        
      p = new NamespacePermission("starksm/Project1", "r---");
      implied = pc.implies(p);
      assertEquals(p.toString(), true, implied);
      p = new NamespacePermission("starksm/Project1/Documents/Folder1", "r---");
      implied = pc.implies(p);
      assertEquals(p.toString(), true, implied);
      p = new NamespacePermission("starksm/Project1/Documents/readme.html", "r---");
      implied = pc.implies(p);
      assertEquals(p.toString(), true, implied);
      p = new NamespacePermission("starksm/Project1/Documents/readme.html", "rw--");
      implied = pc.implies(p);
      assertEquals(p.toString(), true, implied);
      p = new NamespacePermission("starksm/Project1/Documents/readme.html", "-w--");
      implied = pc.implies(p);
      assertEquals(p.toString(), true, implied);
      p = new NamespacePermission("starksm/Project1/Documents/Public/readme.html", "r---");
      implied = pc.implies(p);
      assertEquals(p.toString(), true, implied);
      p = new NamespacePermission("starksm/Project1/Documents/Public/readme.html", "rw--");
      implied = pc.implies(p);
      //assertEquals(p.toString(), true, implied);
   }

   /**
    * Test the NamespacePermissionCollection implies method for various
    * permission that should NOT be implied by the setup PermissionCollection.
    */
   public void testNotImplied()
   {
      NamespacePermission p = new NamespacePermission("Project1/Drawings/view1.jpg", "r---");
      boolean implied = pc.implies(p);
      assertEquals(p.toString(), false, implied);
      p = new NamespacePermission("Project1/Documents/view1.jpg", "r---");
      implied = pc.implies(p);
      assertEquals(p.toString(), false, implied);
      p = new NamespacePermission("starksm/Project1/Documents/readme.html", "rw-d");
      implied = pc.implies(p);
      assertEquals(p.toString(), false, implied);
      p = new NamespacePermission("starksm/Project1/Documents", "rw--");
      implied = pc.implies(p);
      assertEquals(p.toString(), false, implied);
      p = new NamespacePermission("starksm/Project1/Documents/Public/Private/readme.html", "r---");
      implied = pc.implies(p);
      assertEquals(p.toString(), false, implied);
      p = new NamespacePermission("starksm/Project1/Documents/Public/Private/readme.html", "rw--");
      implied = pc.implies(p);
      assertEquals(p.toString(), false, implied);
      p = new NamespacePermission("starksm/Project1/Documents/Folder1/readme.html", "rw--");
      implied = pc.implies(p);
      assertEquals(p.toString(), false, implied);
      p = new NamespacePermission("Project1/Documents/Public/starksm/.bashrc", "r---");
      implied = pc.implies(p);
      assertEquals(p.toString(), false, implied);
   }

   public static void main(String[] args) throws Exception
   {
      NamespacePermissionsUnitTestCase tst = new NamespacePermissionsUnitTestCase("main");
      tst.setUp();
      tst.testImplied();
      tst.testNotImplied();
   }
}
