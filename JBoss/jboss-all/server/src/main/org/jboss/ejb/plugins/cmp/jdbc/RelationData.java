/**
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.ejb.plugins.cmp.jdbc;

import java.util.HashSet;
import java.util.Set;
import javax.ejb.EJBException;
import org.jboss.ejb.plugins.cmp.jdbc.bridge.JDBCCMRFieldBridge;

/**
 * This class holds data about one relationship. It maintains a lists of
 * which relations have been added and removed. When the transaction is
 * committed these list are retrieved and used to update the relation table.   
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.4 $
 */
public class RelationData {
   private JDBCCMRFieldBridge leftCMRField;
   private JDBCCMRFieldBridge rightCMRField;
   
   public Set addedRelations = new HashSet();
   public Set removedRelations = new HashSet();
   public Set notRelatedPairs = new HashSet();
   
   public RelationData(
         JDBCCMRFieldBridge leftCMRField,
         JDBCCMRFieldBridge rightCMRField) {

      this.leftCMRField = leftCMRField;
      this.rightCMRField = rightCMRField;
   }
   
   public JDBCCMRFieldBridge getLeftCMRField() {
      return leftCMRField;
   }
   
   public JDBCCMRFieldBridge getRightCMRField() {
      return rightCMRField;
   }

   public void addRelation(
         JDBCCMRFieldBridge leftCMRField, Object leftId, 
         JDBCCMRFieldBridge rightCMRField, Object rightId) {
      
      // only need to bother if neither side has a foreign key
      if(!leftCMRField.hasForeignKey() && !rightCMRField.hasForeignKey()) {
         RelationPair pair = createRelationPair(
               leftCMRField, leftId, rightCMRField, rightId);
         if(removedRelations.contains(pair)) {
            // we were going to remove this relation
            // and now we are adding it.  Just
            // remove it from the remove set and we are ok.
            removedRelations.remove(pair);
         } else {
            addedRelations.add(pair);

            // if pair was specifically marked as 
            // not related, remove it to the not
            // related set.  See below.
            if(notRelatedPairs.contains(pair)) {
               notRelatedPairs.remove(pair);
            }
         }
      }
   }
   
   public void removeRelation(
         JDBCCMRFieldBridge leftCMRField, Object leftId, 
         JDBCCMRFieldBridge rightCMRField, Object rightId) {

      // only need to bother if neither side has a foreign key
      if(!leftCMRField.hasForeignKey() && !rightCMRField.hasForeignKey()) {
         RelationPair pair = createRelationPair(
               leftCMRField, leftId, rightCMRField, rightId);
         if(addedRelations.contains(pair)) {
            // we were going to add this relation
            // and now we are removing it.  Just
            // remove it from the add set and we are ok.
            addedRelations.remove(pair);
            
            // add it to the set of not related pairs
            // so if remove is called again it is not 
            // added to the remove list. This avoids 
            // an extra 'DELETE FROM...' query.
            // This happend when a object is moved from
            // one relation to another. See 
            // JDBCCMRFieldBridge.createRelationLinks
            notRelatedPairs.add(pair);
         } else {
            // if pair is related (not not related)
            // add it to the remove set.  See above.
            if(!notRelatedPairs.contains(pair)) {
               removedRelations.add(pair);
            }
         }
      }
   }
   
   private RelationPair createRelationPair(
         JDBCCMRFieldBridge leftCMRField, Object leftId, 
         JDBCCMRFieldBridge rightCMRField, Object rightId) {
            
      if(this.leftCMRField == leftCMRField && 
            this.rightCMRField == rightCMRField) {

         return new RelationPair(leftCMRField, leftId, rightCMRField, rightId);
      }
      if(this.leftCMRField == rightCMRField &&
            this.rightCMRField == leftCMRField) {

         return new RelationPair(rightCMRField, rightId, leftCMRField, leftId);
      }   
      throw new EJBException("Error: cmrFields are of wrong type");
   }
}


