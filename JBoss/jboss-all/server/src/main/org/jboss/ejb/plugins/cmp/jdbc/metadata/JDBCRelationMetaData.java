/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb.plugins.cmp.jdbc.metadata;

import java.util.ArrayList;
import java.util.Iterator;
import javax.ejb.EJBException;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.sql.DataSource;
import org.jboss.deployment.DeploymentException;
import org.jboss.metadata.MetaData;
import org.jboss.metadata.RelationMetaData;
import org.jboss.metadata.RelationshipRoleMetaData;
import org.w3c.dom.Element;

/**
 * This class represents one ejb-relation element in the ejb-jar.xml file. 
 * Most properties of this class are immutable.  The mutable properties
 * have set methods.
 *    
 * @author <a href="mailto:dain@daingroup.com">Dain Sundstrom</a>
 * @author <a href="mailto:heiko.rupp@cellent.de">Heiko W. Rupp</a>
 * @version $Revision: 1.14.2.5 $
 */
public final class JDBCRelationMetaData {
   private final static int TABLE = 1;
   private final static int FOREIGN_KEY = 2;
   
   /** Name of the relation. Loaded from the ejb-relation-name element. */
   private final String relationName;

   /** 
    * The left jdbc relationship role. Loaded from an ejb-relationship-role.
    * Left/right assignment is completely arbitrary.
    */
   private final JDBCRelationshipRoleMetaData left;

   /** 
    * The right relationship role. Loaded from an ejb-relationship-role.
    * Left/right assignment is completely arbitrary.
    */
   private final JDBCRelationshipRoleMetaData right;
   
   /**
    * The mapping style for this relation (i.e., TABLE or FOREIGN_KEY).
    */
   private final int mappingStyle;
   
   /** data source name in jndi */
   private final String dataSourceName;
   
   /** This is a cache of the datasource object. */
   private transient DataSource dataSource;
   
   /** type mapping used for the relation table */
   private final JDBCTypeMappingMetaData datasourceMapping;
   
   /** the name of the table to use for this bean */
   private final String tableName;
   
   /** does the table exist */
   private boolean tableExists;

   /** should we create the table when deployed */
   private final boolean createTable;
   
   /** should we drop the table when deployed */
   private final boolean removeTable;
   
   /**
	* What commands should be issued directly after creation
	* of a table?
	*/
   private final ArrayList tablePostCreateCmd;
   
   /** should we use 'SELECT ... FOR UPDATE' syntax? */
   private final boolean rowLocking;
   
   /** should the table have a primary key constraint? */
   private final boolean primaryKeyConstraint;
   
   /** is the relationship read-only? */
   private final boolean readOnly;
   
   /** how long is read valid */
   private final int readTimeOut;
      
   /**
    * Constructs jdbc relation meta data with the data from the relation
    * metadata loaded from the ejb-jar.xml file.
    *
    * @param jdbcApplication used to retrieve the entities of this relation
    * @param relationMetaData relation meta data loaded from the ejb-jar.xml
    * file
    */
   public JDBCRelationMetaData(
         JDBCApplicationMetaData jdbcApplication,
         RelationMetaData relationMetaData) throws DeploymentException {

      relationName = relationMetaData.getRelationName();
      
      RelationshipRoleMetaData leftRole = 
            relationMetaData.getLeftRelationshipRole();
      RelationshipRoleMetaData rightRole =
            relationMetaData.getRightRelationshipRole();

      // set the default mapping style
      if(leftRole.isMultiplicityMany() && rightRole.isMultiplicityMany()) {
         mappingStyle = TABLE;
      } else {
         mappingStyle = FOREIGN_KEY;
      }

      dataSourceName = null;
      datasourceMapping = null;
      createTable = false;
      removeTable = false;
      rowLocking = false;
      primaryKeyConstraint = false;
      readOnly = false;
      readTimeOut = -1;      

      left = new JDBCRelationshipRoleMetaData(
                  this, 
                  jdbcApplication,
                  leftRole);
                  
      right = new JDBCRelationshipRoleMetaData(
                  this, 
                  jdbcApplication,      
                  rightRole); 
      left.init(right);
      right.init(left);

      if(mappingStyle == TABLE) {
         tableName = createDefaultTableName();
         tablePostCreateCmd = getDefaultTablePostCreateCmd();
      } else {
         tableName = null;
         tablePostCreateCmd = null;
      }
   }

   /**
    * Constructs relation meta data with the data contained in the ejb-relation
    * element or the defaults element from a jbosscmp-jdbc xml file. Optional
    * values of the xml element that are not present are loaded from the
    * defaultValues parameter.
    *
    * @param jdbcApplication used to retrieve type mappings in table mapping
    * style
    * @param element the xml Element which contains the metadata about this
    * relation
    * @param defaultValues the JDBCApplicationMetaData which contains the values
    * for optional elements of the element
    * @throws DeploymentException if the xml element is not semantically correct
    */
   public JDBCRelationMetaData(
         JDBCApplicationMetaData jdbcApplication,
         Element element,
         JDBCRelationMetaData defaultValues) throws DeploymentException {
      
      relationName = defaultValues.getRelationName();
      mappingStyle = loadMappingStyle(element, defaultValues);
      
      // post-table-create commands
	  Element posttc = MetaData.getOptionalChild(element,"post-table-create");	  
	  if (posttc!=null) {		  
	 	Iterator it = MetaData.getChildrenByTagName(posttc,"sql-statement");
		tablePostCreateCmd = new ArrayList();
		while (it.hasNext()) {
		  Element etmp = (Element)it.next();  	
		  tablePostCreateCmd.add(MetaData.getElementContent(etmp));
		}
   
	  }
	  else {
		 tablePostCreateCmd = defaultValues.getDefaultTablePostCreateCmd();  
	  }
	      
      
      
      
      // read-only
      String readOnlyString = MetaData.getOptionalChildContent(
            element, "read-only");
      if(readOnlyString != null) {
         readOnly = Boolean.valueOf(readOnlyString).booleanValue();
      } else {
         readOnly = defaultValues.isReadOnly();
      }

      // read-time-out
      String readTimeOutString = MetaData.getOptionalChildContent(
            element, "read-time-out");
      if(readTimeOutString != null) {
         try {
            readTimeOut = Integer.parseInt(readTimeOutString);
         } catch (NumberFormatException e) {
            throw new DeploymentException("Invalid number format in " +
                  "read-time-out '" + readTimeOutString + "': " + e);
         }
      } else {
         readTimeOut = defaultValues.getReadTimeOut();
      }      


      //
      // Load all of the table options. defaults and relation-table-mapping 
      // will have these elements, and foreign-key will get the default values.
      //
      Element mappingElement = getMappingElement(element);

      // datasource name
      String dataSourceNameString = MetaData.getOptionalChildContent(
            mappingElement, "datasource");
      if(dataSourceNameString != null) {
         dataSourceName = dataSourceNameString;
      } else {
         dataSourceName = defaultValues.getDataSourceName();
      }
      
      // get the type mapping for this datasource (optional, but always 
      // set in standardjbosscmp-jdbc.xml)
      String datasourceMappingString = MetaData.getOptionalChildContent(
            mappingElement, "datasource-mapping");      
      if(datasourceMappingString != null) {
         datasourceMapping = jdbcApplication.getTypeMappingByName(
               datasourceMappingString);
      
         if(datasourceMapping == null) {
            throw new DeploymentException("Error in jbosscmp-jdbc.xml : " +
                  "datasource-mapping " + datasourceMappingString + 
                  " not found");
         }
      } else {
         datasourceMapping = defaultValues.getTypeMapping();
      }
      
      // get table name
      String tableNameString = MetaData.getOptionalChildContent(
            mappingElement, "table-name");
      if(tableNameString == null) {
         tableNameString = defaultValues.getDefaultTableName();
         if(tableNameString == null) {
            // use defaultValues to create default, because left/right 
            // have not been assigned yet, and values used to generate
            // default table name never change
            tableNameString = defaultValues.createDefaultTableName();
         }
      }
      tableName = tableNameString;
         
      // create table?  If not provided, keep default.
      String createString = MetaData.getOptionalChildContent(
            mappingElement, "create-table");
      if(createString != null) {
         createTable = Boolean.valueOf(createString).booleanValue();
      } else {
         createTable = defaultValues.getCreateTable();
      }
         
      // remove table?  If not provided, keep default.
      String removeString = MetaData.getOptionalChildContent(
            mappingElement, "remove-table");
      if(removeString != null) {
         removeTable = Boolean.valueOf(removeString).booleanValue();
      } else {
         removeTable = defaultValues.getRemoveTable();
      }

      // select for update
      String sForUpString = MetaData.getOptionalChildContent(
            mappingElement, "row-locking");
      if(sForUpString != null) {
         rowLocking = !isReadOnly() && 
               (Boolean.valueOf(sForUpString).booleanValue());
      } else {
         rowLocking = defaultValues.hasRowLocking();
      }

      // primary key constraint?  If not provided, keep default.
      String pkString = MetaData.getOptionalChildContent(
            mappingElement, "pk-constraint");
      if(pkString != null) {
         primaryKeyConstraint = Boolean.valueOf(pkString).booleanValue();
      } else {
         primaryKeyConstraint = defaultValues.hasPrimaryKeyConstraint();
      }
 
      //
      // load metadata for each specified role
      //
      JDBCRelationshipRoleMetaData defaultLeft =
            defaultValues.getLeftRelationshipRole();
      JDBCRelationshipRoleMetaData defaultRight =
            defaultValues.getRightRelationshipRole();
      
      if(!MetaData.getChildrenByTagName(
            element, "ejb-relationship-role").hasNext()) {

         // no roles specified use the defaults
         left = new JDBCRelationshipRoleMetaData(
                     this,
                     jdbcApplication,                     
                     element,
                     defaultLeft);
                     
         right = new JDBCRelationshipRoleMetaData(
                     this,
                     jdbcApplication,
                     element,
                     defaultRight);

         left.init(right);
         right.init(left);
      } else {
         Element leftElement = 
               getEJBRelationshipRoleElement(element, defaultLeft);
         left = new JDBCRelationshipRoleMetaData(
                     this,
                     jdbcApplication,
                     leftElement,
                     defaultLeft);

         Element rightElement = 
               getEJBRelationshipRoleElement(element, defaultRight);
         right = new JDBCRelationshipRoleMetaData(
                     this,
                     jdbcApplication,
                     rightElement,
                     defaultRight);

         left.init(right, leftElement);
         right.init(left, rightElement);
      }

      // at least one side of a fk relation must have keys
      if(isForeignKeyMappingStyle() && 
            left.getKeyFields().isEmpty() &&
            right.getKeyFields().isEmpty()) {
         throw new DeploymentException("Atleast one role of a foreign-key " +
               "mapped relationship must have key fields " + 
               "(or <primkey-field> is missing from ejb-jar.xml): " +
               "ejb-relation-name=" + relationName);
      }

      // both sides of a table relation must have keys
      if(isTableMappingStyle() && 
            (left.getKeyFields().isEmpty() ||
            right.getKeyFields().isEmpty())) {
         throw new DeploymentException("Both roles of a relation-table " +
               "mapped relationship must have key fields: " +
               "ejb-relation-name=" + relationName);
      }
   }

   private int loadMappingStyle(Element element,
         JDBCRelationMetaData defaultValues) throws DeploymentException {

      // if defaults check for preferred-relation-mapping
      if("defaults".equals(element.getTagName())) {
         // set mapping style based on preferred-relation-mapping (if possible) 
         String perferredRelationMapping = MetaData.getOptionalChildContent(
               element, "preferred-relation-mapping");

         if("relation-table".equals(perferredRelationMapping) ||
               defaultValues.isManyToMany()) {
            return TABLE;
         } else {
            return FOREIGN_KEY;
         }
      }

      // check for table mapping style
      if(MetaData.getOptionalChild(element, "relation-table-mapping") != null) {
         return TABLE;
      }

      // check for foreign-key mapping style
      if(MetaData.getOptionalChild(element, "foreign-key-mapping") != null) {
         if(defaultValues.isManyToMany()) {
            throw new DeploymentException("Foreign key mapping-style " +
                  "is not allowed for many-to-many relationsips.");
         }
         return FOREIGN_KEY;
      }

      // no mapping style element, will use defaultValues
      return defaultValues.mappingStyle;
   }

   private static Element getMappingElement(Element element)
         throws DeploymentException {

      // if defaults check for preferred-relation-mapping
      if("defaults".equals(element.getTagName())) {
         return element;
      }

      // check for table mapping style
      Element tableMappingElement = 
            MetaData.getOptionalChild(element, "relation-table-mapping");
      if(tableMappingElement != null) {
         return tableMappingElement;
      }

      // check for foreign-key mapping style
      Element foreignKeyMappingElement = 
            MetaData.getOptionalChild(element, "foreign-key-mapping");
      if(foreignKeyMappingElement != null) {
         return foreignKeyMappingElement;
      }
      return null;
   }
 
   private static Element getEJBRelationshipRoleElement(
         Element element,
         JDBCRelationshipRoleMetaData defaultRole) throws DeploymentException {

      String roleName = defaultRole.getRelationshipRoleName();

      if(roleName == null)
         throw new DeploymentException("No ejb-relationship-role-name element found");

      Iterator iter = MetaData.getChildrenByTagName(
            element, "ejb-relationship-role");
      if(!iter.hasNext()) {
         throw new DeploymentException("No ejb-relationship-role " + 
               "elements found");
      }

      Element roleElement = null;
      for(int i=0; iter.hasNext(); i++) {
         // only 2 roles are allowed
         if(i > 1) {
            throw new DeploymentException("Expected only 2 " +
                  "ejb-relationship-role but found more then 2");
         }
         
         Element tempElement = (Element)iter.next();
         if(roleName.equals(MetaData.getUniqueChildContent(
               tempElement, "ejb-relationship-role-name"))) {
            roleElement = tempElement;
         }
      }

      if(roleElement == null) {
         throw new DeploymentException("An ejb-relationship-role element was " +
               "not found for role '" + roleName + "'");
      }
      return roleElement;
   }

   /** 
    * Gets the relation name. 
    * Relation name is loaded from the ejb-relation-name element.
    * @return the name of this relation
    */
   public String getRelationName() {
      return relationName;
   }
   
   /** 
    * Gets the left jdbc relationship role. 
    * The relationship role is loaded from an ejb-relationship-role.
    * Left/right assignment is completely arbitrary.
    * @return the left JDBCRelationshipRoleMetaData
    */
   public JDBCRelationshipRoleMetaData getLeftRelationshipRole() {
      return left;
   }

   /** 
    * Gets the right jdbc relationship role.
    * The relationship role is loaded from an ejb-relationship-role.
    * Left/right assignment is completely arbitrary.
    * @return the right JDBCRelationshipRoleMetaData
    */
   public JDBCRelationshipRoleMetaData getRightRelationshipRole() {
      return right;
   }
   
   /** 
    * Gets the relationship role related to the specified role.
    * @param role the relationship role that the related role is desired
    * @return the relationship role related to the specified role.
    * right role of this relation
    */
   public JDBCRelationshipRoleMetaData getOtherRelationshipRole(
      JDBCRelationshipRoleMetaData role) {

      if(left == role) {
         return right;
      } else if(right == role) {
         return left;
      } else {
         throw new IllegalArgumentException("Specified role is not the left " +
               "or right role. role=" + role);
      }
   }
   
   /**
    * Should this relation be mapped to a relation table.
    * @return true if this relation is mapped to a table
    */
   public boolean isTableMappingStyle() {
      return mappingStyle == TABLE;
   }
   
   /**
    * Should this relation use foreign keys for storage.
    * @return true if this relation is mapped to foreign keys
    */
   public boolean isForeignKeyMappingStyle() {
      return mappingStyle == FOREIGN_KEY;
   }
   
   /**
    * Gets the name of the datasource in jndi for this entity
    * @return the name of datasource in jndi
    */
   private String getDataSourceName() {
      return dataSourceName;
   }

   /**
    * Gets the jdbc type mapping for this entity
    * @return the jdbc type mapping for this entity
    */
   public JDBCTypeMappingMetaData getTypeMapping() {
      return datasourceMapping;
   }
   
   /**
    * Gets the name of the relation table.
    * @return the name of the relation table to which is relation is mapped
    */
   public String getDefaultTableName() {
      return tableName;
   }
   
   /**
    * Gets the (user-defined) SQL commands that should be 
    * issued to the db after table creation.
    * @return the SQL command
    */
   private ArrayList getDefaultTablePostCreateCmd() {
   		return tablePostCreateCmd;
   }
   
   /** 
    * Does the table exist yet? This does not mean that table has been created
    * by the appilcation, or the the database metadata has been checked for the
    * existance of the table, but that at this point the table is assumed to 
    * exist.
    * @return true if the table exists
    */
   public boolean getTableExists() {
      return tableExists;
   }

   /** 
    * Sets table exists flag.
    */
   public void setTableExists(boolean tableExists) {
      this.tableExists = tableExists;
   }

   /**
    * Should the relation table be created on startup.
    * @return true if the store mananager should attempt to create the 
    * relation table
    */
   public boolean getCreateTable() {
      return createTable;
   }
   
   /**
    * Should the relation table be removed on shutdown.
    * @return true if the store mananager should attempt to remove the
    * relation table
    */
   public boolean getRemoveTable() {
      return removeTable;
   }
   
   /**
    * When the relation table is created, should it have a primary key
    * constraint.
    * @return true if the store mananager should add a primary key constraint 
    * to the the create table sql statement
    */
   public boolean hasPrimaryKeyConstraint() {
      return primaryKeyConstraint;
   }
   
   /**
    * Is this relation read-only?
    */
   public boolean isReadOnly() {
      return readOnly;
   }
   
   /**
    * Gets the read time out length.
    */
   public int getReadTimeOut() {
      return readTimeOut;
   }
   
   /**
    * Should select queries do row locking
    */
   public boolean hasRowLocking() {
      return rowLocking;
   }
   
   private String createDefaultTableName() {
      String defaultTableName = left.getEntity().getName();
      if(left.getCMRFieldName() != null) {
         defaultTableName += "_" + left.getCMRFieldName();
      }
      defaultTableName += "_" + right.getEntity().getName();
      if(right.getCMRFieldName() != null) {
         defaultTableName += "_" + right.getCMRFieldName();
      }
      return defaultTableName;
   }
   
   private boolean isManyToMany() {
      return left.isMultiplicityMany() && right.isMultiplicityMany();
   }

   public synchronized DataSource getDataSource() {
      if(dataSource == null) {
         try {
            InitialContext context = new InitialContext();
            dataSource = (DataSource)context.lookup(dataSourceName);
         } catch(NamingException e) {
            throw new EJBException("Data source for relationship named " +
                  relationName + " not found " + dataSourceName);
         }
      }
      return dataSource;
   }
}
