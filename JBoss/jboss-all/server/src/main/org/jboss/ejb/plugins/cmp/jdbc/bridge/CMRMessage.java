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
 * @author <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
 * @version $Revision: 1.6.2.3 $
 */
public final class CMRMessage implements Serializable
{
   private static int nextOrdinal = 0;
   private static final CMRMessage[] VALUES = new CMRMessage[5];

   public static final CMRMessage GET_RELATED_ID = new CMRMessage("GET_RELATED_ID");
   public static final CMRMessage ADD_RELATION = new CMRMessage("ADD_RELATION");
   public static final CMRMessage REMOVE_RELATION = new CMRMessage("REMOVE_RELATION");
   public static final CMRMessage SCHEDULE_FOR_CASCADE_DELETE = new CMRMessage("CASCADE_DELETE");
   public static final CMRMessage SCHEDULE_FOR_BATCH_CASCADE_DELETE = new CMRMessage("BATCH_CASCADE_DELETE");


   private final transient String name;
   private final int ordinal;

   private CMRMessage(String name)
   {
      this.name = name;
      this.ordinal = nextOrdinal++;
      VALUES[ordinal] = this;
   }

   public String toString()
   {
      return name;
   }

   Object readResolve() throws ObjectStreamException
   {
      return VALUES[ordinal];
   }

}


