/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.tm;

import javax.transaction.xa.Xid;


/**
 *  This object encapsulates the ID of a transaction.
 *  This implementation is immutable and always serializable at runtime.
 *
 *  @see TransactionImpl
 *  @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 *  @author <a href="mailto:osh@sparre.dk">Ole Husgaard</a>
 *  @version $Revision: 1.2.2.2 $
 */
public class XidImpl
   implements Xid, java.io.Serializable
{
   // Constants -----------------------------------------------------

   public static final int JBOSS_FORMAT_ID = 0x0101;

   // Attributes ----------------------------------------------------

   /**
    *  Hash code of this instance. This is really a sequence number.
    */
   private final int hash;

   /**
    *  Global transaction id of this instance.
    *  The coding of this class depends on the fact that this variable is
    *  initialized in the constructor and never modified. References to
    *  this array are never given away, instead a clone is delivered.
    */
   private final byte[] globalId;

   /**
    *  Branch qualifier of this instance.
    *  This identifies the branch of a transaction.
    */
   private final byte[] branchId;

   // Static --------------------------------------------------------


   /**
    *  Return a string that describes any Xid instance.
    */
   static String toString(Xid id) {
      if (id == null)
         return "[NULL Xid]";

      String s = id.getClass().getName();
      s = s.substring(s.lastIndexOf('.') + 1);
      s = s + " [FormatId=" + id.getFormatId() +
              ", GlobalId=" + new String(id.getGlobalTransactionId()).trim() +
              ", BranchQual=" + new String(id.getBranchQualifier()).trim()+"]";

      return s;
   }

   // Constructors --------------------------------------------------

   /**
    *  Create a new instance.
    */
   public XidImpl(byte[] globalId, byte[] branchId, int hash)
   {
      this.globalId = globalId;
      this.branchId = branchId;
      this.hash = hash;
   }

   /**
    *  Create a new branch of an existing global transaction ID.
    *
    *  @param xid The transaction ID to create a new branch of.
    *  @param branchId The ID of the new branch.
    *
    */
   public XidImpl(final Xid xid, final byte[] branchId)
   {
      if (xid instanceof XidImpl) 
      {
         this.hash = ((XidImpl)xid).hash;
         this.globalId = ((XidImpl)xid).globalId; // reuse array instance, we never modify.
      } // end of if ()
      else
      {
         this.hash = xid.hashCode();
         this.globalId = xid.getGlobalTransactionId();
      } // end of else
      
      this.branchId = branchId;
   }

   // Public --------------------------------------------------------

   // Xid implementation --------------------------------------------

   /**
    *  Return the global transaction id of this transaction.
    */
   public byte[] getGlobalTransactionId()
   {
      return (byte[])globalId.clone();
   }

   /**
    *  Return the branch qualifier of this transaction.
    */
   public byte[] getBranchQualifier()
   {
      if (branchId.length == 0)
         return branchId; // Zero length arrays are immutable.
      else
         return (byte[])branchId.clone();
   }

   /**
    *  Return the format identifier of this transaction.
    *
    *  The format identifier augments the global id and specifies
    *  how the global id and branch qualifier should be interpreted.
    */
   public int getFormatId() {
      // The id we return here should be different from all other transaction
      // implementations.
      // Known IDs are:
      // -1:     Sometimes used to denote a null transaction id.
      // 0:      OSI TP (javadoc states OSI CCR, but that is a bit misleading
      //         as OSI CCR doesn't even have ACID properties. But OSI CCR and
      //         OSI TP do have the same id format.)
      // 1:      Was used by early betas of jBoss.
      // 0x0101: The JBOSS_FORMAT_ID we use here.
      // 0xBB14: Used by JONAS.
      // 0xBB20: Used by JONAS.

      return JBOSS_FORMAT_ID;
   }

   /**
    *  Compare for equality.
    *
    *  Instances are considered equal if they are both instances of XidImpl,
    *  and if they have the same global transaction id and transaction
    *  branch qualifier.
    */
   public boolean equals(Object obj)
   {
      if(obj==this) 
         return true;
      if (obj instanceof XidImpl) {
         XidImpl other = (XidImpl)obj;

         if (globalId.length != other.globalId.length ||
             branchId.length != other.branchId.length)
            return false;

         for (int i = 0; i < globalId.length; ++i)
            if (globalId[i] != other.globalId[i])
               return false;

         for (int i = 0; i < branchId.length; ++i)
            if (branchId[i] != other.branchId[i])
               return false;

         return true;
      }
      return false;
   }

   public int hashCode()
   {
      return hash;
   }

   public String toString()
   {
      return toString(this);
   }

   // Package protected ---------------------------------------------

   /**
    *  Return the global transaction id of this transaction.
    *  Unlike the {@link #getGlobalTransactionId()} method, this one
    *  returns a reference to the global id byte array that may <em>not</em>
    *  be changed.
    */
   byte[] getInternalGlobalTransactionId()
   {
      return globalId;
   }

   
   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}

