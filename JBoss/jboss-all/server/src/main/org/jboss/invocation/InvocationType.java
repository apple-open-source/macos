/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.invocation;

import java.io.Serializable;
import java.io.ObjectStreamException;

/** Type safe enumeration used for to identify the invocation types.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.2.2.1 $
 */
public final class InvocationType implements Serializable
{
   /** Serial Version Identifier. @since 1.2 */
   private static final long serialVersionUID = 6460196085190851775L;

   /** The max ordinal value in use for the InvocationType enums. When you add a
    * new key enum value you must assign it an ordinal value of the current
    * MAX_TYPE_ID+1 and update the MAX_TYPE_ID value.
    */
   private static final int MAX_TYPE_ID = 3;

   /** The array of InvocationKey indexed by ordinal value of the key */
   private static final InvocationType[] values = new InvocationType[MAX_TYPE_ID+1];

   public static final InvocationType REMOTE =
         new InvocationType("REMOTE", 0);
   public static final InvocationType LOCAL =
         new InvocationType("LOCAL", 1);
   public static final InvocationType HOME =
         new InvocationType("HOME", 2);
   public static final InvocationType LOCALHOME =
         new InvocationType("LOCALHOME", 3);

   private final transient String name;

   // this is the only value serialized
   private final int ordinal;

   private InvocationType(String name, int ordinal)
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
