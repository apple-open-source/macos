/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.interfaces;

import java.io.Serializable;

/**
 *  Holder class that knows about a set of HA(sub)Partition currently
 *  building the overall cluster. Exchanged between HASessionState
 *  instances to share the same knowledge.
 *
 *  @see SubPartitionInfo
 *  @see org.jboss.ha.hasessionstate.interfaces.HASessionState
 *  @see org.jboss.ha.hasessionstate.server.HASessionStateImpl
 *
 *  @author <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>
 *  @version $Revision: 1.2.4.2 $
 */
public class SubPartitionsInfo implements Serializable, Cloneable
{
   // Constants -----------------------------------------------------
   /** The serialVersionUID
    * @since 1.2
    */ 
   private static final long serialVersionUID = 3231573521328800529L;

   // Attributes ----------------------------------------------------
   
    public SubPartitionInfo[] partitions = null;    
    protected long groupId = 0;

    // Static --------------------------------------------------------
   
   // Constructors --------------------------------------------------
   
    public SubPartitionsInfo () {}

   // Public --------------------------------------------------------
   
    /**
     * return the next distinct id for a new group
     */    
    public long getNextGroupId ()
    {
       return groupId++;
    }
    
    /**
     * Returns the {@link SubPartitionInfo} instance in this group that has the given name.
     */    
    public SubPartitionInfo getSubPartitionWithName (String name)
    {
       if (partitions != null)
       {
         for (int i=0; i<partitions.length; i++)
            if ((partitions[i]).containsNode (name))
               return partitions[i];
       }

       return null;
    }
    
   // Cloneable implementation ----------------------------------------------
   
    public Object clone ()
    {
       SubPartitionsInfo theClone = new SubPartitionsInfo ();
       
       if (partitions != null)
       {
          theClone.partitions = new SubPartitionInfo[partitions.length];
         for (int i=0; i<partitions.length; i++)
            theClone.partitions[i] = (SubPartitionInfo)partitions[i].clone ();
       }
       
       theClone.groupId = groupId;            
       
       return theClone;
       
    }
    
   // Object overrides ---------------------------------------------------
   
    public String toString ()
    {
       String result = null;
       
       if (partitions == null)
          result = "{null}";
       else
       {
          result = "{";
          for (int i=0; i<partitions.length; i++)
             result+= "\n " + partitions[i].toString ();
          result+= "\n}";
       }
       
       return result;
    }

    // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
    
}
