/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.invocation;

import java.io.Serializable;
import java.io.ObjectStreamException;

/** Type safe enumeration used for to identify the payloads.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public final class PayloadKey implements Serializable
{
   /** Serial Version Identifier. @since 1.1.4.1 */
   private static final long serialVersionUID = 5436722659170811314L;

   /** The max ordinal value in use for the PayloadKey enums. When you add a
    * new key enum value you must assign it an ordinal value of the current
    * MAX_KEY_ID+1 and update the MAX_KEY_ID value.
    */
   private static final int MAX_KEY_ID = 3;

   /** The array of InvocationKey indexed by ordinal value of the key */
   private static final PayloadKey[] values = new PayloadKey[MAX_KEY_ID+1];

   /** Put me in the transient map, not part of payload. */
   public final static PayloadKey TRANSIENT = new PayloadKey("TRANSIENT", 0);
   
   /** Do not serialize me, part of payload as is. */
   public final static PayloadKey AS_IS = new PayloadKey("AS_IS", 1);

   /** Put me in the payload map. */
   public final static PayloadKey PAYLOAD = new PayloadKey("PAYLOAD", 2);

   private final transient String name;

   // this is the only value serialized
   private final int ordinal;
 
   private PayloadKey(String name, int ordinal)
   {
      this.name = name;
      this.ordinal = ordinal;
      values[ordinal] = this;
   }

   public String toString()
   {
      return name;
   }

   Object readResolve() throws ObjectStreamException
   {
      return values[ordinal];
   }
}
