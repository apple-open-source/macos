/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.metadata;

import org.w3c.dom.Element;
import org.jboss.deployment.DeploymentException;

/** 
 * Represents one ejb-relationship-role element found in the ejb-jar.xml
 * file's ejb-relation elements.
 *
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @version $Revision: 1.5 $
 */
public class RelationshipRoleMetaData extends MetaData {
   // one is one
   private static int ONE = 1;
   // and two is many :)
   private static int MANY = 2;
   
   /**
    * Role name
    */
   private String relationshipRoleName;
   
   /**
    * The relation to which the role belongs.
    */
    private RelationMetaData relationMetaData;
   
   /**
    * Multiplicity of role, ONE or MANY.
    */
   private int multiplicity;
   
   /**
    * Should this entity be deleted when related entity is deleted.
    */
   private boolean cascadeDelete;
   
   /**
    * Name of the entity that has this role.
    */
   private String entityName;
   
   /**
    * Name of the entity's cmr field for this role.
    */
   private String cmrFieldName;
   
   /**
    * Type of the cmr field (i.e., collection or set)
    */
   private String cmrFieldType;

   public RelationshipRoleMetaData(RelationMetaData relationMetaData) {
      this.relationMetaData = relationMetaData;
   }
   
   /**
    * Gets the relationship role name
    */
   public String getRelationshipRoleName() {
      return relationshipRoleName;
   }

   /**
    * Gets the relation meta data to which the role belongs.
    * @returns the relation to which the relationship role belongs
    */
   public RelationMetaData getRelationMetaData() {
      return relationMetaData;
   }
   
   /**
    * Gets the related role's metadata
    */
   public RelationshipRoleMetaData getRelatedRoleMetaData() {
      return relationMetaData.getOtherRelationshipRole(this);
   }
   
   /**
    * Checks if the multiplicity is one.
    */
   public boolean isMultiplicityOne() {
      return multiplicity == ONE;
   }
   
   /**
    * Checks if the multiplicity is many.
    */
   public boolean isMultiplicityMany() {
      return multiplicity == MANY;
   }
   
   /**
    * Should this entity be deleted when related entity is deleted.
    */
   public boolean isCascadeDelete() {
      return cascadeDelete;
   }
   
   /**
    * Gets the name of the entity that has this role.
    */
   public String getEntityName() {
      return entityName;
   }
   
   /**
    * Gets the name of the entity's cmr field for this role.
    */
   public String getCMRFieldName() {
      return cmrFieldName;
   }
   
   /**
    * Gets the type of the cmr field (i.e., collection or set)
    */
   public String getCMRFieldType() {
      return cmrFieldType;
   }
   
   public void importEjbJarXml (Element element) throws DeploymentException {
      // ejb-relationship-role-name?
      relationshipRoleName = 
            getOptionalChildContent(element, "ejb-relationship-role-name");
      
      // multiplicity
      String multiplicityString = 
            getUniqueChildContent(element, "multiplicity");
      if("One".equals(multiplicityString)) {
         multiplicity = ONE;
      } else if("Many".equals(multiplicityString)) {
         multiplicity = MANY;
      } else {
         throw new DeploymentException("multiplicity must be exactaly 'One' " +
               "or 'Many' but is " + multiplicityString + "; this is case " +
               "sensitive");
      }
      
      // cascade-delete? 
      if(getOptionalChild(element, "cascade-delete") != null) {
         cascadeDelete = true;
      }
      
      // relationship-role-source
      Element relationshipRoleSourceElement = 
            getUniqueChild(element, "relationship-role-source");
      entityName = 
            getUniqueChildContent(relationshipRoleSourceElement, "ejb-name");
      
      // cmr-field?
      Element cmrFieldElement = getOptionalChild(element, "cmr-field");
      if(cmrFieldElement != null) {
         // cmr-field-name
         cmrFieldName = 
               getUniqueChildContent(cmrFieldElement, "cmr-field-name");
         
         // cmr-field-type?
         cmrFieldType =
               getOptionalChildContent(cmrFieldElement, "cmr-field-type");
         if(cmrFieldType != null &&
               !cmrFieldType.equals("java.util.Collection") && 
               !cmrFieldType.equals("java.util.Set")) {

            throw new DeploymentException("cmr-field-type should be " +
                  "java.util.Collection or java.util.Set but is " + 
                  cmrFieldType);
         }
      }
   }
}
