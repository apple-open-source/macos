/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc.bridge;

import java.io.Serializable;
import java.io.ObjectStreamException;

/**
 * Type safe enumeration of message objects.
 * Used by optmistic lock to lock fields and its values dependeding
 * on the strategy used.
 *
 * @author <a href="mailto:aloubyansky@hotmail.com">Alex Loubyansky</a>
 * @version $Revision: 1.1.2.2 $
 */
public final class CMPMessage
   implements Serializable
{
   // Constants ------------------------------------------------
   private static int nextOrdinal = 0;
   private static final CMPMessage[] VALUES = new CMPMessage[5];

   public static final CMPMessage CHANGED = new CMPMessage("CHANGED");
   public static final CMPMessage ACCESSED = new CMPMessage("ACCESSED");

   private final transient String name;
   private final int ordinal;

   // Constructor ----------------------------------------------
   private CMPMessage(String name)
   {
      this.name = name;
      this.ordinal = nextOrdinal++;
      VALUES[ordinal] = this;
   }

   // Public ---------------------------------------------------
   public String toString()
   {
      return name;
   }

   // Package --------------------------------------------------
   Object readResolve()
      throws ObjectStreamException
   {
      return VALUES[ordinal];
   }
}
