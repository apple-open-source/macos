package org.jboss.test.perf.interfaces;
// EntityPK.java

public class Entity2PK implements java.io.Serializable
{
   public int key1;
   public String key2;
   public Double key3;
   
   public Entity2PK()
   {
   }
   
   public Entity2PK(int key1, String key2, Double key3)
   {
      this.key1 = key1;
      this.key2 = key2;
      this.key3 = key3;
   }
   
   public boolean equals(Object obj)
   {
      Entity2PK key = (Entity2PK) obj;
      boolean equals = key1 == key.key1
         && key2.equals(key.key2) && key3.equals(key.key3);
      return equals;
   }
   public int hashCode()
   {
      return key1 + key2.hashCode() + key3.hashCode();
   }
   
   public String toString()
   {
      return "Entity2PK[" + key1 + ',' + key2 + ',' + key3 + "]";
   }
   
}
