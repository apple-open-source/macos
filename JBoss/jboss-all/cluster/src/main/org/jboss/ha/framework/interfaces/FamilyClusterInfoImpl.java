/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ha.framework.interfaces;

import java.util.ArrayList;
import org.jboss.ha.framework.interfaces.FamilyClusterInfo;

/**
 * Default implementation of FamilyClusterInfo
 *
 * @see org.jboss.ha.framework.interfaces.FamilyClusterInfo
 * @see org.jboss.ha.framework.interfaces.ClusteringTargetsRepository
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 */
public class FamilyClusterInfoImpl implements FamilyClusterInfo
{
   
   // Constants -----------------------------------------------------
   
   // Attributes ----------------------------------------------------
   
   public String familyName = null;
   ArrayList targets = null;
   long currentViewId = 0;
   boolean isViewMembersInSyncWithViewId = false;
   
   int cursor = FamilyClusterInfo.UNINITIALIZED_CURSOR;
   Object arbitraryObject = null;
      
   // Static --------------------------------------------------------
    
   // Constructors --------------------------------------------------
   
   private FamilyClusterInfoImpl (){ }
   
   protected FamilyClusterInfoImpl (String familyName, ArrayList targets, long viewId)
   {
      this.familyName = familyName;
      this.targets = targets;
      this.currentViewId = viewId;
      
      this.isViewMembersInSyncWithViewId = false;
   }

   // Public --------------------------------------------------------
   
   // FamilyClusterInfo implementation ----------------------------------------------
   
   public String getFamilyName () { return this.familyName; }      
   public ArrayList getTargets () { return this.targets; }
   public long getCurrentViewId () { return this.currentViewId; }
   public int getCursor () { return this.cursor; }
   public int setCursor (int cursor) { return (this.cursor = cursor);}
   public Object getObject () { return this.arbitraryObject; }
   public Object setObject (Object whatever) { this.arbitraryObject = whatever; return this.arbitraryObject; }
   
   public ArrayList removeDeadTarget(Object target)
   {
      synchronized (this)
      {
         this.targets.remove (target);
         this.isViewMembersInSyncWithViewId = false;         
      }
      return this.targets;
   }
   
   public ArrayList updateClusterInfo (ArrayList targets, long viewId)
   {
      synchronized (this)
      {
         this.targets = targets;
         this.currentViewId = viewId;
         this.isViewMembersInSyncWithViewId = true;
      }
      return this.targets;
   }
      
   public boolean currentMembershipInSyncWithViewId ()
   {
      return this.isViewMembersInSyncWithViewId;
   }
   
   public void resetView ()
   {
      this.currentViewId = -1;
      this.isViewMembersInSyncWithViewId = false;
   }
      
   // Object overrides ---------------------------------------------------
   
   public int hashCode()
   {
      return this.familyName.hashCode ();
   }
   
   public boolean equals (Object o)
   {
      if (o instanceof FamilyClusterInfoImpl)
      {
         FamilyClusterInfoImpl fr = (FamilyClusterInfoImpl)o;
         return fr.familyName == this.familyName;
      }
      else
         return false;         
   }

   public String toString()
   {
      StringBuffer tmp = new StringBuffer(super.toString());
      tmp.append("{familyName=");
      tmp.append(familyName);
      tmp.append(",targets=");
      tmp.append(targets);
      tmp.append(",currentViewId=");
      tmp.append(currentViewId);
      tmp.append(",isViewMembersInSyncWithViewId=");
      tmp.append(isViewMembersInSyncWithViewId);
      tmp.append(",cursor=");
      tmp.append(cursor);
      tmp.append(",arbitraryObject=");
      tmp.append(arbitraryObject);
      tmp.append("}");
      return tmp.toString();
   }
   // Package protected ---------------------------------------------
   
   // Protected -----------------------------------------------------
   
   // Private -------------------------------------------------------
   
   // Inner classes -------------------------------------------------
   
}
