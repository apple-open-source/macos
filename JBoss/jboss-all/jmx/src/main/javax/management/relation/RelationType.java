/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package javax.management.relation;

import java.io.Serializable;

import java.util.List;

/**
 * This interface is implemented by a class that represents a relation.<p>
 *
 * The class {@link RelationTypeSupport} is available to help
 * implement this interface.<p>
 *
 * A relation type has a name and a list of role info objects for the
 * relation.<p>
 * 
 * A relation type has to registered in the relation service. This is done
 * either by using createRelationType() to get a RelationTypeSupport
 * object kepy in the relation service, or by using addRelationType()
 * to add an external relation type to the relation service.
 *
 * @author  <a href="mailto:Adrian.Brock@HappeningTimes.com">Adrian Brock</a>.
 * @version $Revision: 1.1 $
 *
 */
public interface RelationType
  extends Serializable
{
   // Constants ---------------------------------------------------

   // Public ------------------------------------------------------

   /**
    * Retrieves the name of this relation type.
    *
    * @return the name.
    */
   public String getRelationTypeName();

   /**
    * Retrieves the list of role definitions in this relation type.<p>
    *
    * The return value is a list of RoleInfo objects. The list must be
    * an ArrayList.
    *
    * @return the list of Role Infos.
    */
   public List getRoleInfos();

   /**
    * Retrieves the role info for a role name.<p>
    *
    * @return the role info or null.
    * @exception IllegalArgumentException for a null role info name.
    * @exception RoleInfoNotFoundException for no role info with the
    *            passed name in the relation type.
    */
   public RoleInfo getRoleInfo(String roleInfoName)
     throws IllegalArgumentException, RoleInfoNotFoundException;
}
