package org.jboss.mq.pm;

/**
 * @created    August 16, 2001
 */
public class Tx implements Comparable, java.io.Serializable, java.io.Externalizable {

   long             value = 0;

   public Tx() {
   }

   public Tx( long value ) {
      this.value = value;
   }

   public void setValue( long tx ) {
      value = tx;
   }

   public int hashCode() {
      return ( int )( value ^ ( value >> 32 ) );
   }

   public int compareTo( Tx anotherLong ) {
      long thisVal = this.value;
      long anotherVal = anotherLong.value;
      return ( thisVal < anotherVal ? -1 : ( thisVal == anotherVal ? 0 : 1 ) );
   }

   public int compareTo( Object o ) {
      return compareTo( ( Tx )o );
   }

   public void readExternal( java.io.ObjectInput in )
      throws java.io.IOException {
      value = in.readLong();
   }

   public void writeExternal( java.io.ObjectOutput out )
      throws java.io.IOException {
      out.writeLong( value );
   }

   public long longValue() {
      return value;
   }

   public String toString() {
      return "" + value;
   }
}
