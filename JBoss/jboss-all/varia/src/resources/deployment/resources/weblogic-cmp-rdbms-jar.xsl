<?xml version="1.0"?>

<!-- ================================================================ -->
<!-- Stylesheet for weblogic-cmp-rdbms-jar.xml to jbosscmp-jdbc.xml   -->
<!-- transformation.                                                  -->
<!-- Written by Alexey Loubyansky(loubyansky@ukr.net)                 -->
<!-- ================================================================ -->

<!--
   | ToDo & Notes:
   | - standard WebLogic datasource type mapping table
   | - based on default type mapping convert dbms-column-type element
   |   The element supported only for Oracle
   | - review to check boolean values are handled in case insensitive way
   | - datasource for relationship table isn't defined for now
-->

<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

   <!-- Global parameters -->
   <xsl:param name="ejb-jar">ejb-jar.xml</xsl:param>
   <xsl:param name="standardjbosscmp-jdbc">standardjbosscmp-jdbc.xml</xsl:param>


   <xsl:template match="/">

      <!-- loop through all weblogic-rdbms-jar -->
      <!-- actually there must be only one element -->
      <xsl:for-each select="//weblogic-rdbms-jar">

         <!-- jbosscmp-jdbc -->
         <xsl:element name="jbosscmp-jdbc">

            <!-- defaults -->
            <xsl:call-template name="defaults">
               <xsl:with-param name="weblogic-rdbms-jar" select="."/>
            </xsl:call-template> <!-- defaults -->

            <!-- enterprise-beans -->
            <xsl:call-template name="enterprise-beans">
               <xsl:with-param name="weblogic-rdbms-jar" select="."/>
            </xsl:call-template> <!-- enterprise-beans -->

            <!-- relationships -->
            <xsl:if test="./weblogic-rdbms-relation">
               <xsl:call-template name="relationships">
                  <xsl:with-param name="weblogic-rdbms-jar" select="."/>
               </xsl:call-template> <!-- relationships -->
            </xsl:if>

         </xsl:element> <!-- jbosscmp-jdbc -->
      </xsl:for-each> <!-- weblogic-rdbms-jar -->        
   </xsl:template>


   <!--
      | Template to generate defaults element
   -->
   <xsl:template name="defaults">
      <!-- template parameters -->
      <xsl:param name="weblogic-rdbms-jar"/>

       <!-- defaults -->
       <xsl:element name="defaults">

          <!-- create-table -->
          <xsl:call-template name="create-table">
             <xsl:with-param name="weblogic-rdbms-jar"
                             select="$weblogic-rdbms-jar"/>
          </xsl:call-template> <!-- create-table -->

          <!-- remove-table -->
          <xsl:call-template name="remove-table">
             <xsl:with-param name="weblogic-rdbms-jar"
                             select="$weblogic-rdbms-jar"/>
          </xsl:call-template> <!-- remove-table -->

       </xsl:element> <!-- defaults -->
   </xsl:template> <!-- defaults -->


   <!--
      | Template to generate enterprise-beans element              
   -->
   <xsl:template name="enterprise-beans">
      <!-- template parameters -->
      <xsl:param name="weblogic-rdbms-jar"/>

      <!-- enterprise-beans -->
      <xsl:element name="enterprise-beans">

         <xsl:for-each select="$weblogic-rdbms-jar/weblogic-rdbms-bean">

            <xsl:call-template name="entity">
               <xsl:with-param name="weblogic-rdbms-bean" select="."/>
            </xsl:call-template> <!-- enitity -->

         </xsl:for-each> <!-- weblogic-rdbms-bean -->

      </xsl:element> <!-- enterprise-beans -->
   </xsl:template> <!-- enterprise-beans -->


   <!--
      | Template to generate entity element
   -->
   <xsl:template name="entity">
      <!-- template parameters -->
      <xsl:param name="weblogic-rdbms-bean"/>

      <!-- entity -->
      <xsl:element name="entity">

         <!-- ejb-name -->
         <xsl:copy-of select="$weblogic-rdbms-bean/ejb-name"/>

         <!-- datasource -->
         <!--
            | data-source-name is required element in weblogic-cmp-rdbms-jar.xml
            |  so there is no need to verify its existence
         -->
         <xsl:element name="datasource">
            <xsl:value-of select="$weblogic-rdbms-bean/data-source-name"/>
         </xsl:element> <!-- datasource -->

         <!-- datasource-mapping -->
         <!--
            | datasource-mapping must present if datasource does
            | I don't know how WL does mapping. I've found only java to sql
            | types for oracle.
            | For now the solution is:
            | - check whether datasource is the same as in defaults in
            |   standardjbosscmp-jdbc.xml. Then take the corresponding
            |   datasource-mapping value from defaults
            | - if not, take the datasource value, throw away 'java:/' and use
            |   the tail as a datasource-mapping.
         -->
         <!-- datasource-mapping -->
         <xsl:choose>
            <xsl:when test="document($standardjbosscmp-jdbc)/jbosscmp-jdbc/defaults[datasource=$weblogic-rdbms-bean/data-source-name]">
               <xsl:copy-of select="document($standardjbosscmp-jdbc)/jbosscmp-jdbc/defaults/datasource-mapping"/>
            </xsl:when>
            <xsl:otherwise>
               <xsl:element name="datasource-mapping">
                  <xsl:value-of select="substring-after($weblogic-rdbms-bean/data-source-name, 'java:/')"/>
               </xsl:element> <!-- datasource-mapping-->
            </xsl:otherwise>
         </xsl:choose>

         <!-- table-name -->
         <!--
            | table-name is required in weblogic-rdbms-bean
         -->
         <xsl:copy-of select="$weblogic-rdbms-bean/table-name"/>

         <!-- cmp-field -->
         <xsl:for-each select="$weblogic-rdbms-bean/field-map">
            <xsl:call-template name="cmp-field">
               <xsl:with-param name="field-map" select="."/>
            </xsl:call-template> <!-- cmp-field -->
         </xsl:for-each> <!-- field-map -->

         <!-- load-groups -->
         <xsl:if test="$weblogic-rdbms-bean/field-group">
            <!-- load-groups -->
            <xsl:element name="load-groups">
               <xsl:for-each select="$weblogic-rdbms-bean/field-group">
                  <xsl:call-template name="load-group">
                     <!-- load-group -->
                     <xsl:with-param name="field-group" select="."/>
                  </xsl:call-template> <!-- load-groups  -->
               </xsl:for-each> <!-- field-group -->
            </xsl:element> <!-- load-groups -->
         </xsl:if>

         <!-- queries -->
         <xsl:for-each select="$weblogic-rdbms-bean/weblogic-query">
            <xsl:call-template name="query">
               <xsl:with-param name="weblogic-query" select="."/>
            </xsl:call-template> <!-- query -->
         </xsl:for-each>
        
      </xsl:element> <!-- entity -->
   </xsl:template> <!-- entity -->


   <!--
      | Template to generate query element
   -->
   <xsl:template name="query">
      <!-- template parameters -->
      <xsl:param name="weblogic-query"/>

      <!-- query -->
      <xsl:element name="query">
         <xsl:copy-of select="$weblogic-query/description"/>
         <xsl:copy-of select="$weblogic-query/query-method"/>

         <!--
            | for now all queries are copied
         -->
         <!-- jboss-ql -->
         <xsl:element name="jboss-ql">
            <xsl:value-of select="$weblogic-query/weblogic-ql"/>
         </xsl:element> <!-- jboss-ql -->
      </xsl:element> <!-- query -->
   </xsl:template> <!-- query -->


   <!--
      | Template to generate declared-sql
   -->
   <xsl:template name="declared-sql">
      <!-- template parameters -->
      <xsl:param name="weblogic-ql"/>

      <!-- declared-sql -->
      <xsl:element name="declared-sql">
         <!-- <xsl:value-of select="$weblogic-ql"/> -->
         <!-- select -->
         <xsl:if test="contains($weblogic-ql, 'SELECT')">
            <xsl:element name="select"/>
         </xsl:if>
         <xsl:if test="contains($weblogic-ql, 'FROM')">
            <xsl:element name="from"/>
         </xsl:if>
         <xsl:if test="contains($weblogic-ql, 'WHERE')">
            <xsl:element name="where"/>
         </xsl:if>
         <xsl:if test="contains($weblogic-ql, 'ORDER')">
            <xsl:element name="order"/>
         </xsl:if>
      </xsl:element> <!-- declared-sql -->
   </xsl:template> <!-- declared-sql -->


   <!--
      | Template to generate load-group element
   -->
   <xsl:template name="load-group">
      <!-- template parameters -->
      <xsl:param name="field-group"/>

      <!-- load-group -->
      <xsl:element name="load-group">

         <!-- load-group-name -->
         <xsl:element name="load-group-name">
            <xsl:value-of select="$field-group/group-name"/>
         </xsl:element> <!-- load-group-name -->

         <!-- field-name -->
         <xsl:for-each select="$field-group/cmp-field | $field-group/cmr-field">
            <!-- field-name -->
            <xsl:element name="field-name">
               <xsl:value-of select="."/>
            </xsl:element> <!-- field-name -->
         </xsl:for-each> <!-- cmp-field -->

      </xsl:element> <!-- load-group -->
   </xsl:template> <!-- load-group -->


   <!--
      | Template to generate cmp-field element
   -->
   <xsl:template name="cmp-field">
      <!-- template parameters -->
      <xsl:param name="field-map"/>

      <!-- cmp-field -->
      <xsl:element name="cmp-field">

         <!-- field-name -->
         <xsl:element name="field-name">
            <xsl:value-of select="$field-map/cmp-field"/>
         </xsl:element> <!-- field-name -->

         <!-- column-name -->
         <xsl:element name="column-name">
            <xsl:value-of select="$field-map/dbms-column"/>
         </xsl:element> <!-- column-name -->

      </xsl:element> <!-- cmp-field -->
   </xsl:template> <!-- cmp-field -->


   <!--
      | Template to generate create-table element
   -->
   <xsl:template name="create-table">
      <!-- template parameters -->
      <xsl:param name="weblogic-rdbms-jar"/>

      <!-- create-table -->
      <xsl:element name="create-table">
         <xsl:choose>
            <xsl:when test="$weblogic-rdbms-jar[(create-default-dbms-tables='true')
                                             or (create-default-dbms-tables='True')]">
               <xsl:text>true</xsl:text>
            </xsl:when>
            <xsl:otherwise>
               <xsl:text>false</xsl:text>
            </xsl:otherwise>
         </xsl:choose>
      </xsl:element> <!-- create-table -->
   </xsl:template> <!-- create-table -->


   <!--
      | Template to generate remove-table element
      | note: remove-table and create-table has the same value
   -->
   <xsl:template name="remove-table">
      <!-- template parameters -->
      <xsl:param name="weblogic-rdbms-jar"/>

      <!-- remove-table -->
      <xsl:element name="remove-table">
         <xsl:choose>
            <xsl:when test="$weblogic-rdbms-jar[(create-default-dbms-tables='true')
                                             or (create-default-dbms-tables='True')]">
               <xsl:text>true</xsl:text>
            </xsl:when>
            <xsl:otherwise>
               <xsl:text>false</xsl:text>
            </xsl:otherwise>
         </xsl:choose>
      </xsl:element> <!-- remove-table -->
   </xsl:template> <!-- remove-table -->


   <!--
      | Template to generate relationships element
   -->
   <xsl:template name="relationships">
      <!-- template parameters -->
      <xsl:param name="weblogic-rdbms-jar"/>

      <!-- relationships -->
      <xsl:element name="relationships">
         <xsl:for-each select="$weblogic-rdbms-jar/weblogic-rdbms-relation">
            <!-- ejb-relation -->
            <xsl:element name="ejb-relation">

               <!-- ejb-relation-name -->
               <xsl:element name="ejb-relation-name">
                  <xsl:value-of select="./relation-name"/>
               </xsl:element> <!-- ejb-relation-name -->

               <!-- if table-name then relation-table-mapping
                    else foreign-key-mapping
               -->
               <xsl:choose>
                  <xsl:when test="./table-name">
                     <!-- relation-table-mapping -->
                     <xsl:element name="relation-table-mapping">

                        <!-- table-name -->
                        <xsl:element name="table-name">
                           <xsl:value-of select="./table-name"/>
                        </xsl:element> <!-- table-name -->

                        <!-- create-table -->
                        <xsl:call-template name="create-table">
                           <xsl:with-param name="weblogic-rdbms-jar"
                                           select="$weblogic-rdbms-jar"/>
                        </xsl:call-template> <!-- create-table -->

                        <!-- remove-table -->
                        <xsl:call-template name="remove-table">
                           <xsl:with-param name="weblogic-rdbms-jar"
                                           select="$weblogic-rdbms-jar"/>
                        </xsl:call-template> <!-- remove-table -->

                     </xsl:element> <!-- relation-table-mapping -->
                  </xsl:when> <!-- ./table-name -->

                  <xsl:otherwise>
                     <!-- foreign-key-mapping -->
                     <xsl:element name="foreign-key-mapping"/>
                  </xsl:otherwise>
               </xsl:choose>

               <xsl:choose>
                  <!-- relationship table mapping -->
                  <xsl:when test="count(./weblogic-relationship-role) = 2">
                     <!-- ejb-relationship-role -->
                     <xsl:for-each select="./weblogic-relationship-role">
                        <xsl:call-template name="ejb-relationship-role">
                           <xsl:with-param name="weblogic-relationship-role"
                              select="."/>
                        </xsl:call-template> <!-- ejb-relationship-role -->
                     </xsl:for-each> <!-- weblogic-relationship-role -->
                  </xsl:when>

                  <!-- foreign-key mapping -->
                  <xsl:otherwise>
                     <!--
                        | for o2o and o2m WL specifies weblogic-relationship-role
                        | only for one side while jboss requires both and
                        | mapping is inverted
                     -->
                     <!-- init variables for simplicity -->
                     <xsl:variable name="relationName" select="./relation-name"/>
                     <xsl:variable name="firstRoleName" select="./weblogic-relationship-role/relationship-role-name"/>

                     <!-- construct opposite ejb-relationship-role -->
                     <xsl:element name="ejb-relationship-role">
                        <!-- find the opposite ejb-relationship-role-name in ejb-jar.xml -->
                        <xsl:for-each select="document($ejb-jar)/ejb-jar/relationships/ejb-relation[ejb-relation-name=$relationName]">
                           <xsl:for-each select="./ejb-relationship-role[ejb-relationship-role-name!=$firstRoleName]">
                              <xsl:copy-of select="./ejb-relationship-role-name"/>
                           </xsl:for-each>
                        </xsl:for-each>

                        <xsl:if test="./weblogic-relationship-role/column-map">
                           <!-- key-fields -->
                           <xsl:element name="key-fields">
                              <xsl:for-each select="./weblogic-relationship-role/column-map">
                                 <!-- key-field -->
                                 <xsl:call-template name="key-field">
                                    <xsl:with-param name="column-map" select="."/>
                                 </xsl:call-template> <!-- key-field -->
                              </xsl:for-each> <!-- column-map -->
                           </xsl:element> <!-- key-fields -->
                        </xsl:if> <!-- weblogic-relationship-role/column-map -->
                     </xsl:element> <!-- ejb-relationship-role -->

                     <!-- construct first side of the relationship -->
                     <xsl:element name="ejb-relationship-role">
                        <!-- ejb-relationship-role-name -->
                        <xsl:element name="ejb-relationship-role-name">
                           <xsl:value-of select="$firstRoleName"/>
                        </xsl:element> <!-- ejb-relationship-role-name -->
                        <!-- key-fields -->
                        <xsl:element name="key-fields"/>
                     </xsl:element> <!-- ejb-relationship-role -->
                  </xsl:otherwise>
               </xsl:choose>

            </xsl:element> <!-- ejb-relation -->
         </xsl:for-each> <!-- weblogic-rdbms-relation -->
      </xsl:element> <!-- relationships -->
   </xsl:template> <!-- relationships -->


   <!--
      | Template to generate ejb-relationship-role element
   -->
   <xsl:template name="ejb-relationship-role">
      <!-- template parameters -->
      <xsl:param name="weblogic-relationship-role"/>

      <!-- ejb-relationship-role -->
      <xsl:element name="ejb-relationship-role">

         <!-- ejb-relationship-role-name -->
         <xsl:element name="ejb-relationship-role-name">
            <xsl:value-of select="$weblogic-relationship-role/relationship-role-name"/>
         </xsl:element> <!-- ejb-relationship-role-name -->

         <!-- key-fields -->
         <xsl:choose>
            <!-- wl6.1 key fields mapping -->
            <xsl:when test="$weblogic-relationship-role/column-map">
               <!-- key-fields -->
               <xsl:element name="key-fields">
                  <xsl:for-each select="$weblogic-relationship-role/column-map">
                     <!-- key-field -->
                     <xsl:call-template name="key-field">
                        <xsl:with-param name="column-map" select="."/>
                     </xsl:call-template> <!-- key-field -->
                  </xsl:for-each> <!-- column-map -->
               </xsl:element> <!-- key-fields -->
            </xsl:when>

            <!-- wl7.0 key fields mapping -->
            <xsl:when test="$weblogic-relationship-role/relationship-role-map/column-map">
               <!-- key-fields -->
               <xsl:element name="key-fields">
                  <xsl:for-each select="$weblogic-relationship-role/relationship-role-map/column-map">
                     <!-- key-field -->
                     <xsl:call-template name="key-field">
                        <xsl:with-param name="column-map" select="."/>
                     </xsl:call-template> <!-- key-field -->
                  </xsl:for-each> <!-- column-map -->
               </xsl:element> <!-- key-fields -->
            </xsl:when>            
         </xsl:choose>

         <!-- read-ahead -->
         <!--
            | mapped to on-find
         -->
         <xsl:if test="$weblogic-relationship-role/group-name">
            <!-- read-ahead -->
            <xsl:element name="read-ahead">

               <!-- strategy -->
               <xsl:element name="strategy">
                  <xsl:text>on-find</xsl:text>
               </xsl:element> <!-- strategy -->

               <!-- eager-load-group -->
               <xsl:element name="eager-load-group">
                  <xsl:value-of select="$weblogic-relationship-role/group-name"/>
               </xsl:element> <!-- eager-load-group -->
            </xsl:element> <!-- read-ahead -->
         </xsl:if>

      </xsl:element> <!-- ejb-relationship-role -->
   </xsl:template> <!-- ejb-relationship-role -->


   <!--
      | Template to generate key-field element
   -->
   <xsl:template name="key-field">
      <!-- template parameters -->
      <xsl:param name="column-map"/>

      <!-- key-field -->
      <xsl:element name="key-field">

         <!-- field-name -->
         <xsl:element name="field-name">
            <xsl:value-of select="$column-map/key-column"/>
         </xsl:element> <!-- field-name -->

         <!-- column-name -->
         <xsl:element name="column-name">
            <xsl:value-of select="$column-map/foreign-key-column"/>
         </xsl:element> <!-- coulmn-name -->

      </xsl:element> <!-- key-field -->
   </xsl:template> <!-- key-field -->

</xsl:stylesheet>
