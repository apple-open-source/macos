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
public class Boo implements java.io.Serializable
{
   public String id;
   public String name;
   
   public Boo(String id, String name)
   {
      this.id = id;
      this.name = name;
   }
   
   public String toString()
   {
      return "Boo(" + id + ", \"" + name + "\")";
   }
   
   public boolean equals(Object o)
   {
      return (o instanceof Boo) && (((Boo)o).id.equals(id))
                                && (((Boo)o).name.equals(name));
   }
   
}
