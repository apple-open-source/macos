/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.hasessionstate.server;

import java.util.ArrayList;
import java.util.Iterator;

import org.jboss.ha.framework.interfaces.SubPartitionInfo;
import org.jboss.ha.framework.interfaces.SubPartitionsInfo;

/**
 *   Default implementation of HASessionStateTopologyComputer
 *
 *   @see org.jboss.ha.hasessionstate.interfaces.HASessionState
 *   @author sacha.labourey@cogito-info.ch
 *   @version $Revision: 1.1.4.1 $
 *
 * <p><b>Revisions:</b><br>
 */

public class HASessionStateTopologyComputerImpl implements HASessionStateTopologyComputer
{
   
   protected long nodesPerSubPartition = 0;
   protected String sessionStateIdentifier = null;
   
   /** Creates new HASessionStateTopologyComputerImpl */
   public HASessionStateTopologyComputerImpl ()
   {
   }
   
   public void init (String sessionStateName, long nodesPerSubPartition)
   {
      this.sessionStateIdentifier = sessionStateName;
      this.nodesPerSubPartition = nodesPerSubPartition;
   }
   
   public void start () {}
   
   public SubPartitionsInfo computeNewTopology (SubPartitionsInfo currentTopology, ArrayList newReplicants)
   {
      if (newReplicants.size () < 1)
         currentTopology.partitions = null;
      else if (newReplicants.size () == 1)
      {
         // we are alone! Are we already in a partition? If this is the case, we do not change!
         //
         if (currentTopology.partitions != null)
            currentTopology = computeCompatibleComposition (currentTopology, newReplicants);
         else
         {
            SubPartitionInfo aPartition = new SubPartitionInfo ();
            aPartition.subPartitionName = getSubPartitionName (currentTopology);
            aPartition.memberNodeNames.add (newReplicants.get (0));
            SubPartitionInfo[] thePartition =
            { aPartition };
            currentTopology.partitions = thePartition;
         }
      }
      else if (currentTopology == null || currentTopology.partitions == null)
         // this is the first time we will have to decide of a spliting
         //
         currentTopology = computerFirstComposition (currentTopology, newReplicants);
      else
         // There is a spliting already in place: we will need to take care of it in order to minimize group changes
         // i.e. state transfer that will occur from these changes
         //
         currentTopology = computeCompatibleComposition (currentTopology, newReplicants);
      
      return currentTopology;
      
   }
   
   protected SubPartitionsInfo computerFirstComposition (SubPartitionsInfo splitingInfo, ArrayList replicants)
   {
      int i=0;
      String rep = null;
      ArrayList newConfig = new ArrayList ();
      SubPartitionInfo aPartition = null;
      int grpNumber = 0;
      
      // Build groups sequentially
      //
      for (Iterator reps = replicants.iterator (); reps.hasNext (); i++)
      {
         rep = (String)reps.next ();
         if ( (i%nodesPerSubPartition) == 0 )
         {
            grpNumber++;
            aPartition = new SubPartitionInfo ();
            aPartition.subPartitionName = getSubPartitionName (splitingInfo);
            newConfig.add (aPartition);
         }
         aPartition.memberNodeNames.add (rep);
      }
      
      // we don't like singleton nodes for HA...
      //
      if (aPartition.memberNodeNames.size () == 1)
      {
         rep = (String) aPartition.memberNodeNames.get (0); // get singleton info
         newConfig.remove (grpNumber-1); // remove last singleton group
         aPartition = (SubPartitionInfo)(newConfig.get (grpNumber-1)); // access last built group
         aPartition.memberNodeNames.add (rep); // add singleton to last built group
      }
      
      SubPartitionInfo[] newSpliting = new SubPartitionInfo[1];
      newSpliting = (SubPartitionInfo[]) newConfig.toArray (newSpliting);
      splitingInfo.partitions = newSpliting;
      
      return splitingInfo;
   }
   
   protected SubPartitionsInfo computeCompatibleComposition (SubPartitionsInfo splitingInfo, ArrayList replicants)
   {
      // In a first step, we purge the current spliting to remove dead members
      //
      SubPartitionInfo[] newSpliting = null;
      ArrayList newSubParts = new ArrayList ();
      
      for (int i=0; i<splitingInfo.partitions.length; i++)
      {
         SubPartitionInfo currentSubPart = splitingInfo.partitions[i];
         SubPartitionInfo newCurrent = null;
         Iterator iter = currentSubPart.memberNodeNames.iterator ();
         while (iter.hasNext ())
         {
            String node = (String)iter.next ();
            if (replicants.contains (node))
            {
               if (newCurrent == null)
               {
                  newCurrent = (SubPartitionInfo)currentSubPart.clone ();
                  newCurrent.memberNodeNames.clear ();
               }
               newCurrent.memberNodeNames.add (node);
            }
         }
         if (newCurrent != null)
            newSubParts.add (newCurrent);
      }
      
      // we now create a list of new nodes that are not yet part of any group
      //
      Iterator iter = replicants.iterator ();
      ArrayList newMembersNotInAGroup = new ArrayList ();
      while (iter.hasNext ())
      {
         boolean found = false;
         String aMember = (String)iter.next ();
         Iterator iterNewSubPart = newSubParts.iterator ();
         while (iterNewSubPart.hasNext () && !found)
            if (((SubPartitionInfo)iterNewSubPart.next ()).memberNodeNames.contains (aMember))
               found = true;
         if (!found)
            newMembersNotInAGroup.add (aMember);
      }
      iter = null;
      
      // we now have purged our current sub-partition structure from its dead members
      //  we now check if some sub-partitions need to be merged to remove singleton groups
      // or if there is a group with n>(nodesPerSubPartition) that may be reduced to its ideal size
      //
      
      // we remove elements that are less than the group size and put them in a new sorted list
      //
      ArrayList smallerGroups = new ArrayList ();
      ArrayList correctlySizedGroups = new ArrayList ();
      ArrayList biggerGroups = new ArrayList ();
      
      for (int i=0; i<newSubParts.size (); i++)
      {
         int groupSize = ((SubPartitionInfo)newSubParts.get (i)).memberNodeNames.size ();
         if (groupSize < this.nodesPerSubPartition)
            smallerGroups.add (newSubParts.get (i));
         else if (groupSize > this.nodesPerSubPartition)
            biggerGroups.add (newSubParts.get (i));
         else
            correctlySizedGroups.add (newSubParts.get (i));
      }
      
      // for our algo, we need to sort smallerGroups
      //
      java.util.Collections.sort (smallerGroups);
      
      //
      // Our algo is not perfect and could, for example, take in account, the actual group load in order to minimize
      // the synchronization time
      //
      
      // 1st step: we place newly started nodes (not yet part of a group) in smallerGroups
      // by first feeding small groups
      //
      iter = newMembersNotInAGroup.iterator ();
      while (iter.hasNext ())
      {
         String member = (String)iter.next ();
         SubPartitionInfo target = null;
         if (smallerGroups.size () > 0)
         {
            target = (SubPartitionInfo)smallerGroups.get (0);  // array is sorted
            target.memberNodeNames.add (member);
            if (target.memberNodeNames.size () == this.nodesPerSubPartition)
            {
               // we have a complete sub-partition, we change its owning group
               //
               smallerGroups.remove (0);
               correctlySizedGroups.add (target);
            }
         }
         else
         {
            // we create an singleton group
            //
            target = new SubPartitionInfo ();
            target.setIsNewGroup ();
            target.subPartitionName = getSubPartitionName (splitingInfo);
            target.memberNodeNames.add (member);
            smallerGroups.add (target);
            java.util.Collections.sort (smallerGroups);
         }
      }
      
      // 2nd step: we reduce the size of any too-big sub-partition (biggerGroups)
      // by removing the last component and feeding elements in smallerGroups
      // If smallerGroups is empty, we don't modify biggerGroups (minimize
      // involved state transfer)
      //
      iter = biggerGroups.iterator ();
      while (iter.hasNext ())
      {
         SubPartitionInfo big = (SubPartitionInfo)iter.next ();
         if (smallerGroups.size () > 0)
         {
            String member = (String)big.memberNodeNames.get (big.memberNodeNames.size ()-1); // get last one
            SubPartitionInfo target = null;
            target = (SubPartitionInfo)smallerGroups.get (0);  // array is sorted
            target.memberNodeNames.add (member);
            big.memberNodeNames.remove (big.memberNodeNames.size () -1);
            if (target.memberNodeNames.size () == this.nodesPerSubPartition)
            {
               // we have a complete sub-partition, we change its owning group
               //
               smallerGroups.remove (0);
               correctlySizedGroups.add (target);
            }
         }
      }
      //  biggerGroups is now processed, we can move it to the correctly sized group
      //
      correctlySizedGroups.addAll (biggerGroups);
      
      // 3rd step: we now try to merge sub-partitions belonging to smallerGroups to form bigger groups (up to the
      // max size of a sub-partition). We travel in descending order to keep max granularity when forming groups
      //
      boolean thirdStepFinished = (smallerGroups.size () == 0);
      while (!thirdStepFinished)
      {
         //thirdStepFinished = (smallerGroups.size () == 0);
         SubPartitionInfo current = (SubPartitionInfo)smallerGroups.get (smallerGroups.size ()-1);
         for (int i = smallerGroups.size ()-2; i >= 0; i--)
         {
            // test if the merge is possible
            //
            SubPartitionInfo merger = (SubPartitionInfo)smallerGroups.get (i);
            if ((merger.memberNodeNames.size () + current.memberNodeNames.size ()) <= this.nodesPerSubPartition)
            {
               // it is possible to merge both
               //
               current.merge (merger);
               smallerGroups.remove (i);
               
            }
            // we check if we need to go further or not
            //
            if (current.memberNodeNames.size () == this.nodesPerSubPartition)
               break;
            
         }
         if (current.memberNodeNames.size () > 1)
         {
            // we only move non-singleton groups
            //
            smallerGroups.remove (smallerGroups.size ()-1);
            correctlySizedGroups.add (current);
         }
         
         thirdStepFinished = ( (smallerGroups.size () == 0) ||
         ((smallerGroups.size () == 1) && ( ((SubPartitionInfo)smallerGroups.get (0)).memberNodeNames.size () == 1)) );
         
      }
      
      // 4th step: if smallerGroups is not empty, it means that we have a singleton. In that case,
      // we merge it with the smallest group we can find.
      //
      if (smallerGroups.size () > 0) 
      {
         if (correctlySizedGroups.size ()>0)
         {
         java.util.Collections.sort (correctlySizedGroups);
         SubPartitionInfo merger = (SubPartitionInfo)smallerGroups.get (0);
         SubPartitionInfo master = (SubPartitionInfo)correctlySizedGroups.get (0);
         master.merge (merger);
         }
         else
         {
            // we have a single singleton group!
            //
            correctlySizedGroups.add (smallerGroups.get (0));
         }
      }
      
      // we now commit our new splitting. All members will consequently receive a message indicating
      // that the spliting has changed and act accordingly
      //
      newSpliting = new SubPartitionInfo[1];
      newSpliting = (SubPartitionInfo[])correctlySizedGroups.toArray (newSpliting);
      splitingInfo.partitions = newSpliting;
      
      return splitingInfo;
   }
   
   protected String getSubPartitionName (SubPartitionsInfo manager)
   {
      return this.sessionStateIdentifier + "-Group-" + manager.getNextGroupId ();
   }
   
   // testing of the above algo... can be commented...
   //
   /*
   public static void main (String[] args)
   {
      HASessionStateTopologyComputerImpl tmp = new HASessionStateTopologyComputerImpl ();
      tmp.init ("test", 2);
      tmp.start ();
      
      SubPartitionsInfo splitInfo = new SubPartitionsInfo ();
      ArrayList replic = new ArrayList ();
      ArrayList parts = new ArrayList ();
      splitInfo.partitions = new SubPartitionInfo[1];
      
      //parts.add (new SubPartitionInfo ("SP1", helper_String ("ABC")));
      //parts.add (new SubPartitionInfo ("SP2", helper_String ("DEF")));
      //parts.add (new SubPartitionInfo ("SP3", helper_String ("GHI")));
      
      parts.add (new SubPartitionInfo ("SP4", helper_String ("AB")));
      parts.add (new SubPartitionInfo ("SP5", helper_String ("EF")));
      
      //parts.add (new SubPartitionInfo ("SP1", helper_String ("Axy")));
      //parts.add (new SubPartitionInfo ("SP2", helper_String ("Bmn")));
      //parts.add (new SubPartitionInfo ("SP3", helper_String ("CDE")));
      //parts.add (new SubPartitionInfo ("SP4", helper_String ("FGHI")));
      
      splitInfo.partitions = (SubPartitionInfo[])parts.toArray (splitInfo.partitions);
      replic = new ArrayList (java.util.Arrays.asList (helper_String ("AEF")   ));
      
      System.out.println (replic);
      System.out.println (splitInfo);
      splitInfo = tmp.computeCompatibleComposition (splitInfo, replic);
      System.out.println (splitInfo);
      
   }

   private static String[] helper_String (String letters)
   {
      String[] tabl = new String[letters.length ()];
      for (int i=0; i<letters.length ();i++)
         tabl[i]=letters.substring (i, i+1);
      
      return tabl;
   }
   */
}
