package org.jboss.test.cmp2.simple;

public final class ValueClass implements java.io.Serializable {
   private final int int1;
   private final int int2;

   public ValueClass(int int1, int int2) {
      this.int1 = int1;
      this.int2 = int2;
   }

   public int getInt1() {
      return int1;
   }
   public int getInt2() {
      return int2;
   }

   public boolean equals(Object o) {
      if(o instanceof ValueClass) {
         ValueClass vc = (ValueClass)o;
         return int1 == vc.int1 && int2 == vc.int2;
      }
      return false;
   }

   public int hashCode() {
      int result = 17;
      result = 37*result + int1;
      result = 37*result + int2;
      return result;
   }
}

