/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.iiop.interfaces;

/** 
 * A serializable data object for testing data passed to an EJB.
 */
public class Foo implements java.io.Serializable
{
   public int i;
   public String s;
   
   public Foo(int i, String s)
   {
      this.i = i;
      this.s = s;
   }
   
   public String toString()
   {
      return "Foo(" + i + ", \"" + s + "\")";
   }
   
   public boolean equals(Object o)
   {
      return (o instanceof Foo) && (((Foo)o).i == i)
                                && (((Foo)o).s.equals(s));
   }
   
}
