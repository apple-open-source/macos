<?xml version="1.0"?>

<!--
   | JBoss, the OpenSource EJB server
   |
   | Distributable under LGPL license.
   | See terms of license at gnu.org.
-->

<!--
 This file contains the list of weblogic-cmp-rdbms-jar.xml elements
 supported by WebLogicConverter when porting applications from
 WebLogic 6.1.
 
 @author <a href="mailto:aloubyansky@hotmail.com">Alex Loubyansky</a>

The structure:

<weblogic-elements>

   <weblogic-element>
      <since-version/>
      <element-name/>
      <range-of-values>
         <value/>
      </range-of-values>
      <default-value/>
      <requirements/>
      <parent-elements>
         <element-name/>
      </parent-elements>
      <deployment-file/>
      <function/>
      <examples>
         <example-ref>
            <element-name/>
         </example-ref>
         <example/>
      </examples>
      <jboss-elements>
         <jboss-element>
            <element-name/>
            <deployment-file/>
         </jboss-element>
      </jboss-elements>
   </weblogic-element>

</weblogic-elements>
-->

<weblogic-elements>

   <weblogic-element>
      <since-version>n/a</since-version>
      <element-name>weblogic-rdbms-jar</element-name>
      <range-of-values>
         <value>XML stanza</value>
      </range-of-values>
      <default-value>n/a</default-value>
      <requirements>The root level element of a Weblogic RDBMS CMP deployment descriptor.</requirements>
      <deployment-file>weblogic-cmp-rdbms-jar.xml</deployment-file>
      <function>
         The root level element of a Weblogic RDBMS CMP deployment descriptor.  The
         deployment descriptor contains deployment information for one or more entity
         beans, and an optional set of relations.
      </function>
      <jboss-elements>
         <jboss-element>
            <element-name>jbosscmp-jdbc</element-name>
            <deployment-file>jbosscmp-jdbc.xml</deployment-file>
         </jboss-element>
      </jboss-elements>
   </weblogic-element>

   <weblogic-element>
      <since-version>n/a</since-version>
      <element-name>weblogic-rdbms-bean</element-name>
      <range-of-values>
         <value>XML stanza</value>
      </range-of-values>
      <default-value>n/a</default-value>
      <requirements>The root level element of a Weblogic RDBMS CMP deployment descriptor.</requirements>
      <parent-elements>
         <element-name>weblogic-rdbms-jar</element-name>
      </parent-elements>
      <deployment-file>weblogic-cmp-rdbms-jar.xml</deployment-file>
      <function>
         This element represents a single entity bean that is managed by the WebLogic 
         RDBMS CMP persistence type.
      </function>
      <jboss-elements>
         <jboss-element>
            <element-name>entity</element-name>
            <deployment-file>jbosscmp-jdbc.xml</deployment-file>
         </jboss-element>
      </jboss-elements>
   </weblogic-element>

   <weblogic-element>
      <since-version>n/a</since-version>
      <element-name>ejb-name</element-name>
      <range-of-values>
         <value>Valid name of an EJB.</value>
      </range-of-values>
      <default-value>n/a</default-value>
      <requirements>Must match the ejb-name of the cmp entity bean defined in the ejb-jar.xml.</requirements>
      <parent-elements>
         <element-name>weblogic-rdbms-bean</element-name>
      </parent-elements>
      <deployment-file>weblogic-cmp-rdbms-jar.xml</deployment-file>
      <function>
         The name of the bean.  The contents of this element must match the ejb-name
         element of a cmp entity  bean contained in the ejb-jar.xml descriptor file.
      </function>
      <jboss-elements>
         <jboss-element>
            <element-name>ejb-name</element-name>
            <deployment-file>jbosscmp-jdbc.xml</deployment-file>
         </jboss-element>
      </jboss-elements>
   </weblogic-element>

   <weblogic-element>
      <since-version>n/a</since-version>
      <element-name>data-source-name</element-name>
      <range-of-values>
         <value>Valid name of the data source used for all data base connectivity for this bean.</value>
      </range-of-values>
      <default-value>n/a</default-value>
      <requirements>
         Must be defined as a standard WebLogic Server JDBC data source for database
         connectivity. See Programming WebLogic JDBC for more information.
      </requirements>
      <parent-elements>
         <element-name>weblogic-rdbms-bean</element-name>
      </parent-elements>
      <deployment-file>weblogic-cmp-rdbms-jar.xml</deployment-file>
      <function>
         The data-source-name that specifies the JDBC data source name to be used for all
         database connectivity for this bean.
      </function>
      <jboss-elements>
         <jboss-element>
            <element-name>datasource</element-name>
            <deployment-file>jbosscmp-jdbc.xml</deployment-file>
         </jboss-element>
         <jboss-element>
            <element-name>datasource-mapping</element-name>
            <deployment-file>jbosscmp-jdbc.xml</deployment-file>
         </jboss-element>
      </jboss-elements>
   </weblogic-element>

   <weblogic-element>
      <since-version>n/a</since-version>
      <element-name>table-name</element-name>
      <range-of-values>
         <value>Valid, fully qualified SQL name of the source table in the database.</value>
      </range-of-values>
      <default-value>n/a</default-value>
      <requirements>table-name must be set in all cases.</requirements>
      <parent-elements>
         <element-name>weblogic-rdbms-bean</element-name>
         <element-name>weblogic-rdbms-relation</element-name>
      </parent-elements>
      <deployment-file>weblogic-cmp-rdbms-jar.xml</deployment-file>
      <function>
         The fully qualified SQL name of the source table in the database. Note
         that this needs to be set in all cases. The user defined for the data
         source for this bean must have read and write privileges on this
         table, though not necessarily schema modification privileges.
      </function>
      <jboss-elements>
         <jboss-element>
            <element-name>table-name</element-name>
            <deployment-file>jbosscmp-jdbc.xml</deployment-file>
         </jboss-element>
      </jboss-elements>
   </weblogic-element>

   <weblogic-element>
      <since-version>n/a</since-version>
      <element-name>field-map</element-name>
      <range-of-values>
         <value>Valid name</value>
      </range-of-values>
      <default-value>n/a</default-value>
      <requirements>Field mapped to the column in the database must correspond to a cmp field in the bean.</requirements>
      <parent-elements>
         <element-name>weblogic-rdbms-bean</element-name>
      </parent-elements>
      <deployment-file>weblogic-cmp-rdbms-jar.xml</deployment-file>
      <function>
         The name of the mapped field for a particular column in a database that corresponds
         to a cmp field in the bean instance.
      </function>
      <jboss-elements>
         <jboss-element>
            <element-name>cmp-field</element-name>
            <deployment-file>jbosscmp-jdbc.xml</deployment-file>
         </jboss-element>
      </jboss-elements>
   </weblogic-element>

   <weblogic-element>
      <since-version>n/a</since-version>
      <element-name>cmp-field</element-name>
      <range-of-values>
         <value>Valid name</value>
      </range-of-values>
      <default-value>n/a</default-value>
      <requirements>Field is case sensitive and must match the name of the field in the bean</requirements>
      <parent-elements>
         <element-name>field-map</element-name>
         <element-name>field-group</element-name>
      </parent-elements>
      <deployment-file>weblogic-cmp-rdbms-jar.xml</deployment-file>
      <function>
         This name specifies the mapped field in the bean instance which should be populated
         with information from the database.
      </function>
      <jboss-elements>
         <jboss-element>
            <element-name>field-name</element-name>
            <deployment-file>jbosscmp-jdbc.xml</deployment-file>
         </jboss-element>
      </jboss-elements>
   </weblogic-element>

   <weblogic-element>
      <since-version>n/a</since-version>
      <element-name>dbms-column</element-name>
      <range-of-values>
         <value>Valid name</value>
      </range-of-values>
      <default-value>n/a</default-value>
      <requirements>dbms-column is case maintaining, although not all database are case sensitive.</requirements>
      <parent-elements>
         <element-name>field-map</element-name>
      </parent-elements>
      <deployment-file>weblogic-cmp-rdbms-jar.xml</deployment-file>
      <function>The name of the database column to which the field should be mapped.</function>
      <jboss-elements>
         <jboss-element>
            <element-name>column-name</element-name>
            <deployment-file>jbosscmp-jdbc.xml</deployment-file>
         </jboss-element>
      </jboss-elements>
   </weblogic-element>

   <weblogic-element>
      <since-version>n/a</since-version>
      <element-name>field-group</element-name>
      <range-of-values>
         <value>Valid name</value>
      </range-of-values>
      <default-value>A special group named default is used for finders and relationships that have no group specified.</default-value>
      <requirements>The default group contains all of a bean's cmp-fields, but none of its cmr-fields.</requirements>
      <parent-elements>
         <element-name>weblogic-rdbms-bean</element-name>
         <element-name>weblogic-rdbms-relation</element-name>
      </parent-elements>
      <deployment-file>weblogic-cmp-rdbms-jar.xml</deployment-file>
      <function>
         The field-group element represents a subset of the cmp and cmr-fields of a bean.
         Related fields in a bean can be put into groups that are faulted into memory together
         as a unit. A group can be associated with a finder or relationship, so that when a bean
         is loaded as the result of executing a finder or following a relationship, only the fields
         specified in the group are loaded.
         A field may belong to multiple groups. In this case, the getXXX method for the field
         faults in the first group that contains the field.
      </function>
      <jboss-elements>
         <jboss-element>
            <element-name>load-group</element-name>
            <deployment-file>jbosscmp-jdbc.xml</deployment-file>
         </jboss-element>
      </jboss-elements>
   </weblogic-element>

   <weblogic-element>
      <since-version>n/a</since-version>
      <element-name>group-name</element-name>
      <range-of-values>
         <value>Valid name</value>
      </range-of-values>
      <default-value>n/a</default-value>
      <requirements>n/a</requirements>
      <parent-elements>
         <element-name>field-group</element-name>
         <element-name>finder-query</element-name>
      </parent-elements>
      <deployment-file>weblogic-cmp-rdbms-jar.xml</deployment-file>
      <function>The group-name element specifies the name of a field group.</function>
      <jboss-elements>
         <jboss-element>
            <element-name>load-group-name</element-name>
            <deployment-file>jbosscmp-jdbc.xml</deployment-file>
         </jboss-element>
      </jboss-elements>
   </weblogic-element>

   <weblogic-element>
      <since-version>n/a</since-version>
      <element-name>cmr-field</element-name>
      <range-of-values>
         <value>Valid name</value>
      </range-of-values>
      <default-value>n/a</default-value>
      <requirements>The field referenced in cmr-field must have a matching cmr-field entry in the ejb-jar.xml.</requirements>
      <parent-elements>
         <element-name>field-group</element-name>
      </parent-elements>
      <deployment-file>weblogic-cmp-rdbms-jar.xml</deployment-file>
      <function>The cmr-field element specifies the name of a cmr-field.</function>
      <jboss-elements>
         <jboss-element>
            <element-name>field-name</element-name>
            <deployment-file>jbosscmp-jdbc.xml</deployment-file>
         </jboss-element>
      </jboss-elements>
   </weblogic-element>

   <weblogic-element>
      <since-version>n/a</since-version>
      <element-name>weblogic-rdbms-relation</element-name>
      <range-of-values>
         <value>XML stanza</value>
      </range-of-values>
      <default-value>n/a</default-value>
      <requirements>Represents a single relationship.</requirements>
      <parent-elements>
         <element-name>weblogic-rdbms-jar</element-name>
      </parent-elements>
      <deployment-file>weblogic-cmp-rdbms-jar.xml</deployment-file>
      <function>
         This element represents a single relationship that is managed by
         the WebLogic RDBMS CMP persistence type.
      </function>
      <jboss-elements>
         <jboss-element>
            <element-name>ejb-relation</element-name>
            <deployment-file>jbosscmp-jdbc.xml</deployment-file>
         </jboss-element>
      </jboss-elements>
   </weblogic-element>

   <weblogic-element>
      <since-version>n/a</since-version>
      <element-name>relation-name</element-name>
      <range-of-values>
         <value>Valid name</value>
      </range-of-values>
      <default-value>n/a</default-value>
      <requirements>
         Must match the ejb-relation-name of an ejb-relation in the associated
         ejb-jar.xml descriptor file.
      </requirements>
      <parent-elements>
         <element-name>weblogic-rdbms-relation</element-name>
      </parent-elements>
      <deployment-file>weblogic-cmp-rdbms-jar.xml</deployment-file>
      <function>The relation-name element specifies the name of a relation.</function>
      <jboss-elements>
         <jboss-element>
            <element-name>ejb-relation-name</element-name>
            <deployment-file>jbosscmp-jdbc.xml</deployment-file>
         </jboss-element>
      </jboss-elements>
   </weblogic-element>

   <weblogic-element>
      <since-version>n/a</since-version>
      <element-name>weblogic-relationship-role</element-name>
      <range-of-values>
         <value>Valid name</value>
      </range-of-values>
      <default-value>n/a</default-value>
      <requirements>
         The mapping of a role to a table is specified in the associated weblogic-rdbms-bean
         and ejb-relation elements.
      </requirements>
      <parent-elements>
         <element-name>weblogic-rdbms-relation</element-name>
      </parent-elements>
      <deployment-file>weblogic-cmp-rdbms-jar.xml</deployment-file>
      <function>
         The weblogic-relationship-role element is used to express a mapping from a
         foreign key to a primary key. Only one mapping is specified for one-to-one
         relationships when the relationship is local. However, with a many-to-many
         relationship, you must specify two mappings
         Multiple column mappings are specified for a single role, if the key is complex. No
         column-map is specified if the role is just specifying a group-name.
      </function>
      <jboss-elements>
         <jboss-element>
            <element-name>ejb-relationship-role</element-name>
            <deployment-file>jbosscmp-jdbc.xml</deployment-file>
         </jboss-element>
      </jboss-elements>
   </weblogic-element>

   <weblogic-element>
      <since-version>n/a</since-version>
      <element-name>relationship-role-name</element-name>
      <range-of-values>
         <value>Valid name</value>
      </range-of-values>
      <default-value>n/a</default-value>
      <requirements>
         The name mustmatch the ejb-relationship-role-name of an
         ejb-relationship-role in the associated ejb-jar.xml descriptor file.
      </requirements>
      <parent-elements>
         <element-name>weblogic-relationship-role</element-name>
      </parent-elements>
      <deployment-file>weblogic-cmp-rdbms-jar.xml</deployment-file>
      <function>The relationship-role-name element specifies the name of a relationship role.</function>
      <jboss-elements>
         <jboss-element>
            <element-name>ejb-relationship-role-name</element-name>
            <deployment-file>jbosscmp-jdbc.xml</deployment-file>
         </jboss-element>
      </jboss-elements>
   </weblogic-element>

   <weblogic-element>
      <since-version>n/a</since-version>
      <element-name>column-map</element-name>
      <range-of-values>
         <value>n/a</value>
      </range-of-values>
      <default-value>n/a</default-value>
      <requirements>
         The key-column element is not specified, if the foreign-key-column refers to a
         remote bean.
      </requirements>
      <parent-elements>
         <element-name>weblogic-relationship-role</element-name>
      </parent-elements>
      <deployment-file>weblogic-cmp-rdbms-jar.xml</deployment-file>
      <function>
         This element represents the mapping of a foreign key column in one table in the
         database to a corresponding primary key. The two columns may or may not be in the
         same table. The tables to which the column belong are implicit from the context in
         which the column-map element appears in the deployment descriptor.
      </function>
      <jboss-elements>
         <jboss-element>
            <element-name>key-field</element-name>
            <deployment-file>jbosscmp-jdbc.xml</deployment-file>
         </jboss-element>
      </jboss-elements>
   </weblogic-element>

   <weblogic-element>
      <since-version>n/a</since-version>
      <element-name>foreign-key-column</element-name>
      <range-of-values>
         <value>Valid name</value>
      </range-of-values>
      <default-value>n/a</default-value>
      <requirements>Must correspond to a column of a foreign key.</requirements>
      <parent-elements>
         <element-name>column-map</element-name>
      </parent-elements>
      <deployment-file>weblogic-cmp-rdbms-jar.xml</deployment-file>
      <function>The foreign-key-column element represents a column of a foreign key in the database.</function>
      <jboss-elements>
         <jboss-element>
            <element-name>column-name</element-name>
            <deployment-file>jbosscmp-jdbc.xml</deployment-file>
         </jboss-element>
      </jboss-elements>
   </weblogic-element>

   <weblogic-element>
      <since-version>n/a</since-version>
      <element-name>key-column</element-name>
      <range-of-values>
         <value>Valid name</value>
      </range-of-values>
      <default-value>n/a</default-value>
      <requirements>Must correspond to a column of a primary key.</requirements>
      <parent-elements>
         <element-name>column-map</element-name>
      </parent-elements>
      <deployment-file>weblogic-cmp-rdbms-jar.xml</deployment-file>
      <function>The key-column element represents a column of a primary key in the database.</function>
      <jboss-elements>
         <jboss-element>
            <element-name>field-name</element-name>
            <deployment-file>jbosscmp-jdbc.xml</deployment-file>
         </jboss-element>
      </jboss-elements>
   </weblogic-element>

   <weblogic-element>
      <since-version>n/a</since-version>
      <element-name>create-default-dbms-tables</element-name>
      <range-of-values>
         <value>True</value>
         <value>False</value>
      </range-of-values>
      <default-value>False</default-value>
      <requirements>
         Use this element only for convenience during the development and prototyping phases.
         This is because the Table Schema in the DBMS CREATE statement used will be the
         container's best approximation of the definition. A production environment most likely,
         will require a more precise schema definition.
      </requirements>
      <parent-elements>
         <element-name>weblogic-rdbms-jar</element-name>
      </parent-elements>
      <deployment-file>weblogic-cmp-rdbms-jar.xml</deployment-file>
      <function>
         The create-default-dbms-table element turns on or off a feature that
         automatically creates a default table based on the descriptions in the deployment files
         and the bean class. When set to False, this feature is turned off and table will not
         automatically be generated. When set to True, this feature is turned on and the table is
         automatically created. If TABLE CREATION fails, a Table Not Found error is thrown
         and the table must be created by hand.
      </function>
      <jboss-elements>
         <jboss-element>
            <element-name>create-table</element-name>
            <deployment-file>jbosscmp-jdbc.xml</deployment-file>
         </jboss-element>
         <jboss-element>
            <element-name>remove-table</element-name>
            <deployment-file>jbosscmp-jdbc.xml</deployment-file>
         </jboss-element>
      </jboss-elements>
   </weblogic-element>

</weblogic-elements>
