/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

/**
 * This interface defines the management interface for a relation
 * created internally within the relation service. The relation can
 * have only roles - no attributes or mehods.<p>
 *
 * The relation support managed bean can be created externally, including
 * extending it, and then registered with the relation service.<p>
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.2 $
 */
public interface RelationSupportMBean
  extends Relation
{
   // Constants ---------------------------------------------------

   // Public ------------------------------------------------------

   /**
    * Check to see whether this relation thinks it is in relation service.<p>
    *
    * WARNING: This is not a dynamic check. The flag is set within the
    * relation support object by the relation service, malicious programs
    * may modifiy it to an incorrect value.
    *
    * @return true when it is registered.
    */
   public Boolean isInRelationService();

   /**
    * Set the flag to specify whether this relation is registered with
    * the relation service.<p>
    *
    * WARNING: This method is exposed for management by the relation
    * service. Using this method outside of the relation service does
    * not affect the registration with the relation service.
    *
    * @param value pass true for managed by the relation service, false 
    *        otherwise.
    * @exception IllegalArgumentException for a null value
    */
   public void setRelationServiceManagementFlag(Boolean value);
}
