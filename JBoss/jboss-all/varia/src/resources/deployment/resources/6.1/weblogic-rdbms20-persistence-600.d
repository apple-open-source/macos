<!--
This is the DTD for WebLogic 6.0 RDBMS CMP deployment descriptors.

Copyright (c) 1999-2000 by BEA Systems, Inc. All Rights Reserved.
-->


<!--
The root level element of a Weblogic RDBMS CMP deployment descriptor.  The
deployment descriptor contains deployment information for one or more entity
beans, and an optional set of relations.
-->
<!ELEMENT weblogic-rdbms-jar (
    weblogic-rdbms-bean+,
    weblogic-rdbms-relation*,
    create-default-dbms-tables?,
    validate-db-schema-with?
)>


<!--
This element represents a single entity bean that is managed by the WebLogic 
RDBMS CMP persistence type.
-->
<!ELEMENT weblogic-rdbms-bean (
    ejb-name,
    data-source-name,
    table-name,
    field-map*,
    field-group*,
    weblogic-query*,
    delay-database-insert-until?,
    automatic-key-generation?
)>
 

<!--
The name of the bean.  The contents of this element must match the ejb-name
element of a cmp entity  bean contained in the ejb-jar.xml descriptor file.

Used in: weblogic-rdbms-bean

Example:
    <ejb-name>EmployeeEJB</ejb-name>
-->
<!ELEMENT ejb-name (#PCDATA)>


<!--
The JNDI name of the WebLogic data source which should be used for all
database connectivity for this particular bean. This data source must
be defined in the standard way for WebLogic JDBC data sources. If you
do not know how to do this please consult the WebLogic Server
Documentation.

Used in: weblogic-rdbms-bean

Example:
    <data-source-name>financeDB</data-source-name>
-->
<!ELEMENT data-source-name (#PCDATA)>


<!--
The fully qualified SQL name of the source table in the database. Note
that this needs to be set in all cases. The user defined for the data
source for this bean must have read and write privileges on this
table, though not necessarily schema modification privileges.

Used in: weblogic-rdbms-bean, weblogic-rdbms-relation

Example:
    <table-name>ACCOUNT</table-name>
    <table-name>ACCOUNTING_SCHEMA.ACCOUNT</table-name>
    <table-name>"SELECT".ACCOUNT</table-name>
    <table-name>"ACCOUNT"</table-name>

-->
<!ELEMENT table-name (#PCDATA)>


<!--
The field-map element represents a mapping between a particular column in a 
database and a cmp field in the bean instance.

Used in: weblogic-rdbms-bean

Example:
    <field-map>
        <cmp-field>accountID</cmp-field>
        <dbms-column>ACCOUNT_NUMBER</dbms-column>
    </field-map>
-->
<!ELEMENT field-map (cmp-field, dbms-column, dbms-column-type?)>


<!--
The field in the bean instance which should be populated from the database.
This field is case sensitive and must precisely match the name of the
field in the bean instance. The field referenced in this element must have
a cmp-field entry in the ejb-jar.xml file.

Used in: field-map

Example:
    <cmp-field>balance</cmp-field>
-->
<!ELEMENT cmp-field (#PCDATA)>


<!--
The database column to which the given field should be mapped.
This element is case maintaining, though not all databases are
case sensitive.

Used in: field-map

Example:
    <dbms-column>BALANCE</dbms-column>
-->
<!ELEMENT dbms-column (#PCDATA)>


<!--
The dbms-column-type can be either OracleBlob or OracleClob.
This will map the current field to a Blob or Clob in Oracle database.

Used in: field-map

Since: WLS 6.1

Example:
    <dbms-column-type>OracleBlob</dbms-column-type>
-->
<!ELEMENT dbms-column-type (#PCDATA)>


<!-- 
The column-map element represents a mapping from a foreign key
column in one table in the database to a corresponding primary key
column.  The two columns may or may not be in the same table.  The
tables to which the column belong are implicit from the context in
which the column-map element appears in the deployment descriptor.
The key-column element is not specified if the foreign-key-column 
refers to a 'remote' bean.

Used in: weblogic-relationship-role

Example:
    <column-map>
      <foreign-key-column>manager-id</foreign-key-column>
      <key-column>id</key-column>
    </column-map>

    <column-map>
      <foreign-key-column>remote-customer-id</foreign-key-column>
    <column-map>
-->
<!ELEMENT column-map (foreign-key-column, key-column?)>


<!-- 
The foreign-key-column element represents a column of a foreign key
in the database.

Used in: column-map

Example:
    <foreign-key-column>manager-id</foreign-key-column>
-->
<!ELEMENT foreign-key-column (#PCDATA)>


<!-- 
The key-column element represents a column of a primary key
in the database.

Used in: column-map

Example:
    <key-column>id</key-column>
-->
<!ELEMENT key-column (#PCDATA)>


<!-- 
This element represents a single relationship that is managed by
the WebLogic RDBMS CMP persistence type.  Three general types of
relationship mappings are supported; one for one-to-one, one-to-many,
and many-to-many relations.  We describe each case, and how the tags
are used for each below.

For one-to-one relations, the physical mapping is from a foreign key
in one bean to the primary key of the other.  For example, a
relationship mapping between an Employee bean and another Employee
bean that is his/her manager would look something like this:

    <weblogic-rdbms-relation>
        <relation-name>employee-manager</relation-name>
        <weblogic-relationship-role>
            <relationship-role-name>employee</relationship-role-name>
            <column-map>
                <foreign-key-column>manager-id</foreign-key-column>
                <key-column>id</key-column>
            </column-map>
        </weblogic-relationship-role>
    </weblogic-rdbms-relation>

Note that the example above just contains the physical mapping
information for the relation.  It says that there is a foreign key
column 'manager-id' in the table that the employee role is mapped to
that refers to the primary key column 'id' in the table to which the
other role in this relationship is mapped.  This information is the
same no matter whether the relation is bi-directional or not.  The
mapping of roles to tables is specified in the corresponding
weblogic-rdbms-bean and ejb-relation elements.

For one-to-many relations, the physical mapping is also always from a
foreign key in one bean to the primary key of another. In a
one-to-many relation, the foreign key is always contained in the role
that occupies the 'many' side of the relation.  Thus, the
specification of the relationship-role-name below is a bit redundant,
but is included for uniformity.  Here's an example of a mapping for a
relation between employees and departments that is one-to-many:

    <weblogic-rdbms-relation>
        <relation-name>employee-department</relation-name>
        <weblogic-relationship-role>
            <relationship-role-name>employee</relationship-role-name>
            <column-map>
                <foreign-key-column>dept-id</foreign-key-column>
                <key-column>id</key-column>
            </column-map>
        </weblogic-relationship-role>
    </weblogic-rdbms-relation>

For many-to-many relations, the physical mapping involves a join
table.  Each row in the join table contains two foreign keys that map
to the primary keys of the entities involved in the relation.  A
many-to-many relation called 'friends' between employees could be
represented like this:

    <weblogic-rdbms-relation>
        <relation-name>friends</relation-name>
        <table-name>FRIENDS</table-name>
        <weblogic-relationship-role>
            <relationship-role-name>first-friend</relationship-role-name>
            <column-map>
                <foreign-key-column>first-friend-id</foreign-key-column>
                <key-column>id</key-column>
            </column-map>
        </weblogic-relationship-role>
        <weblogic-relationship-role>
            <relationship-role-name>second-friend</relationship-role-name>
            <column-map>
                <foreign-key-column>second-friend-id</foreign-key-column>
                <key-column>id</key-column>
            </column-map>
        </weblogic-relationship-role>
    </weblogic-rdbms-relation>

In the example above, the FRIENDS table has two columns,
'first-friend-id' and 'second-friend-id'.  Each column contains a
foreign key that designates a particular employee who is a friend of
another employee.  The primary key column of the employee table is
'id'.

Used in: weblogic-rdbms-jar
-->
<!ELEMENT weblogic-rdbms-relation (
    relation-name,
    table-name?,
    weblogic-relationship-role,
    weblogic-relationship-role?  
)>


<!--
The name of a relation.  

When an ejb-relation-name is specified in the ejb-jar.xml descriptor
file, the relation-name must match the ejb-relation-name.

Otherwise, the default relation name should be used.  The default
relation name is formed by concatenating the default role names for a
relationship together, separated by a '-'.  The default role names
must appear in alphabetical order in a default relation name.  For
example, if the role names were 'CustomerEJB.account' and
'AccountEJB.customer', the relation-name would be
'AccountEJB.customer-CustomerEJB.account'.

Used in: weblogic-rdbms-relation

Since: Default relation names added in WLS 6.1, SP2.

Example: 
    <relation-name>employee-manager</relation-name>
-->
<!ELEMENT relation-name (#PCDATA)>


<!-- 
The weblogic-relationship-role element represents the physical mapping
between two roles in a relation.  The mapping of a role to a table is
specified in the associated weblogic-rdbms-bean and ejb-relation
elements.  For a relationship between 'local' beans, multiple column
mappings may be given if the key implementing the relation is a
compound key.  For a 'remote' bean, only a single column-map is 
specified, since the primary key of the remote bean is opaque.  No 
column-map is given if the role is just specifying a group-name.  No
group-name is specified if the relationship is remote.

Used in: weblogic-rdbms-relation

Example: 
    (see weblogic-rdbms-relation for examples using 'local' beans)

    (remote example)
    <weblogic-relationship-role>
      <relationship-role-name>
         account-has-remote-customer
      </relationship-role-name>
      <column-map>
        <foreign-key-column>remote-customer-id</foreign-key-column>
      <column-map>
    </weblogic-relationship-role>
-->
<!ELEMENT weblogic-relationship-role (
    relationship-role-name,
    group-name?,
    column-map*,
    db-cascade-delete?
)>


<!-- 
The name of a relationship role.  

If an ejb-relationship-role-name is specified in the ejb-jar.xml
descriptor file for the role, then the relationship-role-name must
match the ejb-relationship-role-name.

Otherwise, the default name for the role should be used.  The default
name for a relationship role is formed by concatenating the ejb-name
of the bean participating in the role together with the
cmr-field-name, if there is one, separated by a '.' character.

For example, if the bean 'CustomerEJB' has cmr-field 'account', then
the default role name would be CustomerEJB.account.  If there is no
cmr-field-name, then just the ejb-name is used.

Used in: weblogic-relationship-role

Since: Default relationship role names added in WLS 6.1, SP2.

Example: 
    <relationship-role-name>employee</relationship-role-name>
-->
<!ELEMENT relationship-role-name (#PCDATA)>


<!--
The db-cascade-delete tag specifies that a cascade delete will use the
builtin cascade delete facilities of the underlying DBMS.
By default, this feature is turned off and the EJB container removes the
beans involved in a cascade delete by issuing an individual SQL DELETE
statement for each bean.

The purpose of the db-cascade-delete option is to allow an application to
take advantage of a database's builtin suppport for cascade delete, and
possibly improve performance.  If db-cascade-delete is not specified, it is
important not to enable any of the database's cascade delete functionality,
as this will produce incorrect results.

If db-cascade-detele tag is specified in the weblogic-cmp-rdbms-jar.xml, the
cascade-delete tag must be specified in the ejb-jar.xml.

When db-cascade-detele is enabled, additional database table setup is required.
For example, the following setup for the Oracle database table will cascade
delete all of the emps if the dept is deleted in the database.
  CREATE TABLE dept
    (deptno   NUMBER(2) CONSTRAINT pk_dept PRIMARY KEY,
     dname    VARCHAR2(9) );
  CREATE TABLE emp
    (empno    NUMBER(4) PRIMARY KEY,
     ename    VARCHAR2(10),
     deptno   NUMBER(2)   CONSTRAINT fk_deptno
              REFERENCES dept(deptno)
              ON DELETE CASCADE );

Used in: weblogic-relationship-role

Since: WLS 6.1  

Example:
    <db-cascade-delete/>
-->
<!ELEMENT db-cascade-delete EMPTY>


<!-- 
A field-group represents a subset of the cmp and cmr-fields of a bean.
Related fields in a bean can be put into groups which are faulted
into memory together as a unit.  A group can be associated with a
query or relationship, so that when a bean is loaded as the
result of executing a query or following a relationship, only the
fields mentioned in the group are loaded.

A special group named 'default' is used for queriess and relationships
that have no group specified.  By default, the 'default' group contains
all of a bean's cmp-fields and any cmr-fields that add a foreign key to the
persistent state of the bean.

A field may belong to multiple groups.  In this case, the getXXX() method
for the field will fault in the first group (lexically speaking) that 
contains the field.
-->
<!ELEMENT field-group (
    group-name,
    (cmp-field | cmr-field)+
)>

         
<!--
The name of a cmr-field.  The field referenced in this element must have
a matching cmr-field entry in the ejb-jar.xml file.

Used in: field-group

Example:
    <cmr-field>employee</cmr-field>
-->
<!ELEMENT cmr-field (#PCDATA)>


<!--
The name of a field group.

Used in: field-group

Examples:
    <group-name>financial-data</group-name>
    <group-name>medical-data</group-name>
-->
<!ELEMENT group-name (#PCDATA)>

                         
<!-- 
The weblogic-query element provides a way to associate weblogic
specific attributes with a query when this is necessary.  For example,
the weblogic-query element can be used to specify a query that
contains a weblogic specific extension to the ejb-ql language.
Queries that do not need to take advantage of weblogic extensions to
ejb-ql should be specified in the standard ejb-jar.xml descriptor.

The weblogic-query element is also used to associate a field-group
with the query if the query is retrieving an entity bean(s) that
should be pre-loaded into the cache by the query.

Used in: weblogic-rdbms-bean

Example:
    <weblogic-query>
      <description>find all acounts balance is greater than 10000</description>
      <query-method>
        <method-name>findBigAccounts</method-name>
        <method-params>
          <method-param>double</method-param>
        </method-params>
      </query-method>
      <weblogic-ql>WHERE BALANCE>10000 ORDERBY NAME</weblogic-ql>
    </weblogic-query>
-->
<!ELEMENT weblogic-query (
    description?,
    query-method,
    weblogic-ql?,
    group-name?,
    max-elements?,
    include-updates?,
    sql-select-distinct?
)>


<!--
The description element is used to provide text describing the parent element.

Used in: weblogic-query
-->
<!ELEMENT description (#PCDATA)>


<!--
The weblogic-ql tag is used to specify a query that contains weblogic
specific extensions to the ejb-ql language.  Queries that only use
standard ejb-ql language features should be specified in the
ejb-jar.xml deployment descriptor.

Used in: weblogic-query

Example:
    <weblogic-ql>WHERE BALANCE>10000 ORDERBY NAME</weblogic-ql>
-->
<!ELEMENT weblogic-ql (#PCDATA)>


<!--
The method-name tag is used to specify the name of a finder or
ejbSelect method.  The '*' character may not be used as a wildcard.

Used in: query-method

Example:
    <method-name>findByBalance</method-name>
    <method-name>ejbSelectOverdueAccounts</method-name>
-->
<!ELEMENT method-name (#PCDATA)>


<!--
The method-param element contains the fully-qualified Java type
name of a method parameter.

Used in: method-params

Example:
    <method-param>java.lang.String</method-param>
-->
<!ELEMENT method-param (#PCDATA)>

<!--
The method-params element contains an ordered list of the
fully-qualified Java type names of the method parameters.

Used in: query-method

Example:
    <method-params>
      <method-param>java.lang.String</method-param>
      <method-param>java.lang.Integer</method-param>
    </method-params>
-->
<!ELEMENT method-params (method-param*)>


<!-- 
This tag is used to specify the method that is associated with a
weblogic-query.  This tag uses the same format as the ejb-jar.xml 
descriptor.

Example:
    <query-method>
       <method-name>findBigAccounts</method-name>
       <method-params>
          <method-param>double</method-param>
       </method-params>
    <query-method>
-->
<!ELEMENT query-method (method-name, method-params)>


<!--
The max-elements tag is used to specify the maximum number of
elements that should be returned by a multi-valued query.  This
option is similar to the maxRows feature of JDBC.

Used in: weblogic-query

Since: WLS 6.0 SP1

Example:
    <max-elements>100</max-elements>
-->
<!ELEMENT max-elements (#PCDATA)>


<!--
The value of the include-updates element must be 'True' or 'False'.

The include-updates tag is used to specify that updates made during
the current transaction must be reflected in the result of a query.
If include-updates is set to 'True', the container will flush all
changes made by the current transaction to disk before executing the
query.

The default value is 'False', which provides the best performance.

Used in: weblogic-query

Since: WLS 6.1

Example:
    <include-updates>False</include-updates>
-->
<!ELEMENT include-updates (#PCDATA)>


<!--
The value of the sql-select-distinct element must be 'True' or 'False'.

The sql-select-distinct tag is used to control whether the generated
SQL 'SELECT' will contain a 'DISTINCT' qualifier.  Use of the DISTINCT 
qualifier will cause the RDBMS to return unique rows.  

Note:   ORACLE will not allow the use of 'SELECT DISTINCT' in conjuction
          with a 'FOR UPDATE' clause, thus, <sql-select-distinct>True
          CANNOT be used if any Bean in the calling chain has a method with 
             <transaction-isolation> set to 
               <isolation-level>TRANSACTION_READ_COMMITTED_FOR_UPDATE

The default value is 'False'.

Used in: weblogic-query

Since: WLS 6.1SP02

Example:
    <sql-select-distinct>True</sql-select-distinct>
-->
<!ELEMENT sql-select-distinct (#PCDATA)>


<!--
This optional tag is used to install the Automatic Sequence/Key 
Generation facility

Used in: weblogic-rdbms-bean

Since: WLS 6.1  

Example:
  <automatic-key-generation>
    <generator-type>ORACLE</generator-type>
    <generator-name>test_sequence</generator-name>
    <key-cache-size>10</key-cache-size>
  </automatic-key-generation>

Example:
  <automatic-key-generation>
    <generator-type>SQL_SERVER</generator-type>
  </automatic-key-generation>


Example:
  <automatic-key-generation>
    <generator-type>NAMED_SEQUENCE_TABLE</generator-type>
    <generator-name>MY_SEQUENCE_TABLE_NAME</generator-name>
    <key-cache-size>100</key-cache-size>
  </automatic-key-generation>

Notes: This feature is intended for use with Simple (non-Compound)
       Primary Keys.

       The Primary Key cmp-field must be of type:
          java.lang.Integer
	  java.lang.Long

-->
<!ELEMENT automatic-key-generation (
    generator-type,
    generator-name?,
    key-cache-size?
)>


<!--
Specifies the Key Generation Method that will be employed
  'ORACLE'               - use Oracle's SEQUENCE
  'SQL_SERVER'           - use SQL SERVER's IDENTITY column
  'NAMED_SEQUENCE_TABLE' - use USER designated SEQUENCE TABLE
                               User specifies the name of a DBMS Table
                               with the schema (sequence INT)
                               which will be used to hold sequence values

Used in: automatic-key-generation

Since: WLS 6.1

-->
<!ELEMENT generator-type (#PCDATA)>


<!--
If this is DBMS Key Generation, 
Then this may be used to specify the Name of the Generator,
  e.g. for <generator-type>ORACLE
           <generator-name> would be the name of the ORACLE SEQUENCE to use.
                             the SEQUENCE is assumed to already exist in the Database.

If this is <generator-type>NAMED_SEQUENCE_TABLE Key Generation,
Then this would be used to specify the name of the SEQUENCE TABLE to use.
 the NAMED_SEQUENCE_TABLE is assumed to already exist in the Database
 with 1 row.

Used in: automatic-key-generation

Since: WLS 6.1

-->
<!ELEMENT generator-name (#PCDATA)>


<!--
Optional size of key cache.
For <generator-type>ORACLE , this value MUST match the
  Oracle SEQUENCE  INCREMENT value.  If there is a mismatch
  between this value and the Oracle SEQUENCE INCREMENT value,
  then there will likely be duplicate key problems.

For <generator-type>NAMED_SEQUENCE_TABLE , this tells how many keys
  the Container will fetch in a single DBMS call

For <generator-type>SQL_SERVER, this value is ignored

Since: WLS 6.1

Used in: automatic-key-generation

-->
<!ELEMENT key-cache-size (#PCDATA)>


<!-- 

<p>The delay-database-insert-until element is used to specify the
precise time at which a new bean that uses RDBMS CMP is inserted into
the database.  By default, the database insert is done after ejbPostCreate.

<p>Delaying the database insert until after ejbPostCreate is required
when a cmr-field is mapped to a foreign-key column that doesn't allow
null values.  In this case, the cmr-field must be set to a non-null value
in ejbPostCreate before the bean is inserted into the database.  Note that
cmr-fields may not be set during ejbCreate, before the primary key of the
bean is known.

<p>It is also generally advisable to delay the database insert until after
ejbPostCreate if the ejbPostCreate method modifies the persistent
fields of the bean.  This can yield better performance by avoiding an
unnecessary store operation.

<p>For maximum flexibility, developers should avoid creating related
beans in their ejbPostCreate method.  This may make delaying the
database insert impossible if database constraints prevent related
beans from referring to a bean that has not yet been created.

Allowed values:
     ejbCreate - perform database insert immediately after ejbCreate
     ejbPostCreate - perform insert immediately after ejbPostCreate (default)

Used in: weblogic-rdbms-bean

Since: WLS 6.0 SP1

Example:
    <delay-database-insert-until>ejbPostCreate</delay-database-insert-until>
-->
<!ELEMENT delay-database-insert-until (#PCDATA)>


<!--
The value of create-default-dbms-tables may be "True" or "False".
The default value of create-default-dbms-tables is "False"

When this parameter is "True", the following will occur:

For each CMP Bean listed in the jar,  at Deployment time, if there is
no Table in the Database for the Bean, the Container will attempt to
CREATE the Table based on information found in the deployment files and in
the Bean Class.
If TABLE CREATION fails, a 'Table Not Found' Error will be thrown, and
the TABLE must be created by hand.

This feature should be used only for convenience during development
and prototyping as the Table Schema in the DBMS CREATE used will be the Containers 
best approximation, a production enviroment may require a more precise 
schema definition.

Used in: weblogic-rdbms-jar

Since: WLS 6.1  

Example:
    <create-default-dbms-tables>True</create-default-dbms-tables>

-->
<!ELEMENT create-default-dbms-tables (#PCDATA)>
<!--

The value of validate-db-schema-with may be "MetaData" or "TableQuery".
The default value is "TableQuery".

Used in: weblogic-rdbms-jar

Since: WLS 6.1sp2

Example:
    <validate-db-schema-with>MetaData</validate-db-schema-with>
-->
<!ELEMENT validate-db-schema-with (#PCDATA)>






