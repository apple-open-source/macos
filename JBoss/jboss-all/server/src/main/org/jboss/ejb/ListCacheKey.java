/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb;

import java.io.ObjectOutput;
import java.io.ObjectInput;
import java.io.IOException;

/**
 * ListCacheKey extends {@link CacheKey} and holds info about the List that the entity belongs to,
 * it is used with CMP 2.0 for reading ahead.
 *
 * @author <a href="mailto:on@ibis.odessa.ua">Oleg Nitz</a>
 * @version $Revision: 1.2 $
 */
public final class ListCacheKey
extends CacheKey
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   /**
    * The list id.
    */
   private long listId;

   /**
    * The index of this entity in the list.
    */
   private int index;

   // Static --------------------------------------------------------

   // Public --------------------------------------------------------

   public ListCacheKey() {
      // For externalization only
   }

   /**
    * @param listId The list id.
    * @param index The index of this entity in the list.
    */
   public ListCacheKey(Object id, long listId, int index) {
      super(id);
      this.listId = listId;
      this.index = index;
   }

   public long getListId()
   {
      return listId;
   }

   public int getIndex()
   {
      return index;
   }

   // Z implementation ----------------------------------------------

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   public void writeExternal(ObjectOutput out)
      throws IOException
   {
      super.writeExternal(out);
      out.writeLong(listId);
      out.writeInt(index);
   }

   public void readExternal(ObjectInput in)
      throws IOException, ClassNotFoundException
   {
      super.readExternal(in);
      listId = in.readLong();
      index = in.readInt();
   }

   // Inner classes -------------------------------------------------
}
