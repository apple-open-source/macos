/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.iiop.interfaces;

/** 
 * A serializable data object for testing data passed to an EJB.
 * This data object has a sub-object and a transient field. It also
 * defines a writeObject method (which means the corresponding IDL 
 * valuetype custom-marshaled).
 */
public class Zoo implements java.io.Serializable {

   public String id;
   public String name;
   public Zoo inner;
   private transient Object hidden = "hidden";

   public Zoo(String id, String name) {
      this.id = id;
      this.name = name;
      this.inner = null;
   }

   public Zoo(String id, String name, Zoo inner) {
      this.id = id;
      this.name = name;
      this.inner = inner;
   }

   public String toString() {
      return "Zoo(" + id + ", \"" + name + "\"" +
         ((inner == null) ? "" : ", " + inner.toString()) + ")";
   }

   public boolean equals(Object o) {
        return (o instanceof Zoo)
           && (((Zoo)o).id.equals(id))
           && (((Zoo)o).name.equals(name))
           && ((((Zoo)o).inner == null && inner == null)
               || (((Zoo)o).inner != null && ((Zoo)o).inner.equals(inner)));
   }

   private synchronized void writeObject(java.io.ObjectOutputStream s)
      throws java.io.IOException {
      id = id + "!";
      s.defaultWriteObject();
   }
}
