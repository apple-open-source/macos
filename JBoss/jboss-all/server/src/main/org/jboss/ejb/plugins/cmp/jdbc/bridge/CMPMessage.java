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
 * @version $Revision: 1.1.2.1 $
 */
public final class CMPMessage
   implements Serializable
{
   // Constants ------------------------------------------------
   private static int nextOrdinal = 0;
   private static final CMPMessage[] VALUES = new CMPMessage[5];

   public static final CMPMessageKey CMP_MESSAGE_KEY = new CMPMessageKey();

   public static final CMPMessage CREATED = new CMPMessage("CREATED");
   public static final CMPMessage LOADED =  new CMPMessage("LOADED");
   public static final CMPMessage CHANGED = new CMPMessage("CHANGED");
   public static final CMPMessage ACCESSED = new CMPMessage("ACCESSED");
   public static final CMPMessage RESETTED = new CMPMessage("RESETTED");

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

   // Inner ----------------------------------------------------
   private static final class CMPMessageKey
      implements Serializable
   {
      // Constructor -------------------------------------------
      private CMPMessageKey() { }

      // Public ------------------------------------------------
      public String toString()
      {
         return "CMP_MESSAGE_KEY";
      }

      // Package -----------------------------------------------
      Object readResolve() throws ObjectStreamException
      {
         return CMPMessage.CMP_MESSAGE_KEY;
      }
   }
}
