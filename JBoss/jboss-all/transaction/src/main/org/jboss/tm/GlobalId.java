/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.tm;

import java.io.IOException;
import javax.transaction.xa.Xid;


/**
 *  This object encapsulates the global transaction ID of a transaction.
 *  It is similar to an Xid, but holds only the GlobalId part.
 *  This implementation is immutable and always serializable at runtime.
 *
 *  @see XidImpl
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.2.2.1 $
 */
public class GlobalId
   implements java.io.Externalizable
{
   /**
    *  Hash code of this instance. This is really a sequence number.
    */
   private int hash;

   /**
    *  Global transaction id of this instance.
    *  The coding of this class depends on the fact that this variable is
    *  initialized in the constructor and never modified. References to
    *  this array are never given away, instead a clone is delivered.
    */
   private byte[] globalId;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------
   public GlobalId()
   {
      // Used for Externalizable support
   }
   
   /**
    *  Create a new instance. This constructor is public <em>only</em>
    *  to get around a class loader problem; it should be package-private.
    */
   public GlobalId(int hash, byte[] globalId)
   {
      this.hash = hash;
      this.globalId = globalId;
   }


   public GlobalId(Xid xid)
   {
      this.hash = xid.hashCode();
      if (xid instanceof XidImpl) 
      {
         this.globalId = ((XidImpl)xid).getInternalGlobalTransactionId();
      } // end of if ()
      else
      {
         this.globalId = xid.getGlobalTransactionId();
      } // end of else
   }

   // Public --------------------------------------------------------

   /**
    *  Compare for equality.
    *
    *  Instances are considered equal if they both refer to the same
    *  global transaction id.
    */
   public boolean equals(Object obj)
   {
      if (obj instanceof GlobalId) {
         GlobalId other = (GlobalId)obj;

         if (hash != other.hash)
            return false;

         if (globalId == other.globalId)
            return true;

         if (globalId.length != other.globalId.length)
            return false;

         int len = globalId.length;
         for (int i = 0; i < len; ++i)
            if (globalId[i] != other.globalId[i])
               return false;

         return true;
      }
      return false;
   }

   public int hashCode()
   {
      return hash;
   }
   
   // Externalizable implementation ---------------------------------
   public void writeExternal(java.io.ObjectOutput out)
      throws IOException
   {
      out.writeInt(hash);
      out.writeObject(globalId);
   }
   
   public void readExternal(java.io.ObjectInput in)
      throws IOException, ClassNotFoundException
   {
      hash = in.readInt();
      globalId = (byte[])in.readObject();
   }
}

