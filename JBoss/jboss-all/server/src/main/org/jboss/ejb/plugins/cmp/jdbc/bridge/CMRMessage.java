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
 * Type safe enumeration of method object is passed through the invocation
 * interceptor chain and caught by the JDBCRelationInterceptor.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.6.2.1 $
 */
public final class CMRMessage implements Serializable {
   private static int nextOrdinal = 0;
   private static final CMRMessage[] VALUES = new CMRMessage[4];

   public static final CMRMessageKey CMR_MESSAGE_KEY = new CMRMessageKey();

   public static final CMRMessage GET_RELATED_ID = 
         new CMRMessage("GET_RELATED_ID");
   public static final CMRMessage ADD_RELATION = 
         new CMRMessage("ADD_RELATION");
   public static final CMRMessage REMOVE_RELATION =
         new CMRMessage("REMOVE_RELATION");
   public static final CMRMessage INIT_RELATED_CTX =
         new CMRMessage("INIT_RELATED_CTX");


   private final transient String name;
   private final int ordinal;
    
   private CMRMessage(String name) {
      this.name = name;
      this.ordinal = nextOrdinal++;
      VALUES[ordinal] = this;
   }

   public String toString() {
      return name;
   }

   Object readResolve() throws ObjectStreamException {
      return VALUES[ordinal];
   }

   private static final class CMRMessageKey implements Serializable {
      private CMRMessageKey() {
      }
      public String toString() {
         return "CMR_MESSAGE_KEY";
      }
      Object readResolve() throws ObjectStreamException {
         return CMRMessage.CMR_MESSAGE_KEY;
      }
   }
}


