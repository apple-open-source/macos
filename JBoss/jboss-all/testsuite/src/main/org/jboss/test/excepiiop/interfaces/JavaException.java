/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.excepiiop.interfaces;

public class JavaException
   extends Exception
{
   public int i = (int)0;
   public String s = null;
   
   public JavaException ()
   {
   }
   
   public JavaException (int i, String s)
   {
      this.i = i;
      this.s = s;
   }
}
