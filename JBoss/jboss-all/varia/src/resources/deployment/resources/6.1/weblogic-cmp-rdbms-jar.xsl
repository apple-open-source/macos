<?xml version="1.0"?>

<!--
   | Stylesheet for weblogic-cmp-rdbms-jar.xml to jbosscmp-jdbc.xml
   | transformation.
   | WebLogic version: 6.1
   | author: Alexey Loubyansky <aloubyansky@hotmail.com>
-->
<!--
   | Aug-2003 - 
   | author: Marie Foucault <foucault@actoll.com>
   | * The <table-name> and <cmp-field> elements are included into a <table-map> 
   |   elements in somes versions of th weblogic weblogic-cmp-rdbms-jar.xml. add
   |   a rule to check if this element is present or not.
   | * By Default the datasource-mapping is set to data-source-name value so that
   |   the stylesheet check in the standardjbosscmp-jdbc.xml file to find the mapping
   |   associated to the datasource.
-->

<xsl:stylesheet
   version="1.0"
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

   <!-- Global parameters -->
   <xsl:param name="ejb-jar-xml">ejb-jar.xml</xsl:param>
   <xsl:param name="standardjbosscmp-jdbc">standardjbosscmp-jdbc.xml</xsl:param>
   <xsl:param name="remove-table">create-default-dbms-tables</xsl:param>
   <xsl:param name="datasource">data-source-name</xsl:param>
   <xsl:param name="datasource-mapping">data-source-name</xsl:param>

   <!-- global variables -->
   <xsl:variable name="ejb-jar" select="document($ejb-jar-xml)/ejb-jar"/>

   <xsl:template match="/">

      <!-- loop through all weblogic-rdbms-jar (must be only one)-->
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
               </xsl:call-template>
            </xsl:if> <!-- relationships -->

         </xsl:element> <!-- jbosscmp-jdbc -->
      </xsl:for-each> <!-- weblogic-rdbms-jar -->        
   </xsl:template>

   <!--
      | Template: defaults
      | Parameters:
      |    weblogic-rdbms-jar - the weblogic-rdbms-jar element
   -->
   <xsl:template name="defaults">
      <xsl:param name="weblogic-rdbms-jar"/>

       <!-- defaults -->
       <xsl:element name="defaults">

          <!--
             | datasource
             | Using the first found data-source-name
          -->
          <xsl:if test="$weblogic-rdbms-jar//data-source-name">
            <xsl:call-template name="datasource">
               <xsl:with-param name="weblogic-rdbms-jar" select="$weblogic-rdbms-jar//data-source-name[1]"/>
            </xsl:call-template>
          </xsl:if>

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
      | Template: datasource & datasource-mapping
      | Parameters:
      |    data-source-name - the data-source-name specified in WL's DD
      | Logic:
      |    $datasource is equal to 'data-source-name':
      |       if data-source-name starts with 'java:/'
      |       then copy data-source-name as is
      |       else prefix data-source-name with 'java:/'
      |    $datasource is not equal to 'data-source-name':
      |       set datasource to the value of $datasource
      | Note: datasource-mapping is set up only if datasource-mapping is
   -->
   <xsl:template name="datasource">
      <xsl:param name="data-source-name"/>

      <xsl:choose>
         <xsl:when test="$datasource='data-source-name'">
            <xsl:for-each select="document($standardjbosscmp-jdbc)//type-mapping[name=$data-source-name]">
               <xsl:element name="datasource">
                  <xsl:choose>
                     <xsl:when test="starts-with($data-source-name, 'java:/')">
                        <xsl:value-of select="$data-source-name"/>
                     </xsl:when>
                     <xsl:otherwise>
                        <xsl:value-of select="concat('java:/', $data-source-name)"/>
                     </xsl:otherwise>
                  </xsl:choose>
               </xsl:element>

               <!-- datasource-mapping -->
               <xsl:call-template name="datasource-mapping">
                  <xsl:with-param name="data-source-name" select="$data-source-name"/>
               </xsl:call-template>
            </xsl:for-each>
         </xsl:when>
         <xsl:otherwise>
            <xsl:element name="datasource">
               <xsl:value-of select="$datasource"/>
            </xsl:element>

            <!-- datasource-mapping -->
            <xsl:call-template name="datasource-mapping">
               <xsl:with-param name="data-source-name" select="$data-source-name"/>
            </xsl:call-template>
         </xsl:otherwise>
      </xsl:choose>
   </xsl:template> <!-- datasource -->

   <!--
      | Template: datasource-mapping
      | Parameters:
      |    data-source-name - the data-source-name element
      | Logic:
      |    $datasource-mapping is equal to 'data-source-name':
      |       search for type-mapping in standardjbosscmp-jdbc.xml with name
      |       matching data-source-name. if found then create
      |       datasource-mapping with the name equal to data-source-name;
      |       if not found then datasource-mapping is omitted
      |    $datasource-mapping is not equal to 'data-source-name':
      |       datasource-mapping is set up to $datasource-mapping
   -->
   <xsl:template name="datasource-mapping">
      <xsl:param name="data-source-name"/>

      <xsl:choose>
         <xsl:when test="$datasource-mapping='data-source-name'">
            <xsl:choose>
               <xsl:when test="starts-with($data-source-name, 'java:/')">
                  <xsl:for-each select="document($standardjbosscmp-jdbc)//type-mapping[name=substring-after($data-source-name, 'java:/')]">
                     <xsl:element name="datasource-mapping">
                        <xsl:value-of select="./name"/>
                     </xsl:element>
                  </xsl:for-each>
               </xsl:when>
               <xsl:otherwise>
                  <xsl:for-each select="document($standardjbosscmp-jdbc)//type-mapping[name=$data-source-name]">
                     <xsl:element name="datasource-mapping">
                        <xsl:value-of select="./name"/>
                     </xsl:element>
                  </xsl:for-each>
               </xsl:otherwise>
            </xsl:choose>
         </xsl:when>
         <xsl:otherwise>
            <xsl:element name="datasource-mapping">
               <xsl:value-of select="$datasource-mapping"/>
            </xsl:element>
         </xsl:otherwise>
      </xsl:choose>
   </xsl:template> <!-- datasource-mapping -->

   <!--
      | Template: enterprise-beans
      | Parameters:
      |    weblogic-rdbms-jar - the weblogic-rdbms-jar element
   -->
   <xsl:template name="enterprise-beans">
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
      | Template: entity
      | Parameters:
      |    weblogic-rdbms-bean - the weblogic-rdbms-bean element
   -->
   <xsl:template name="entity">
      <xsl:param name="weblogic-rdbms-bean"/>

      <!-- entity -->
      <xsl:element name="entity">

         <!-- ejb-name -->
         <xsl:copy-of select="$weblogic-rdbms-bean/ejb-name"/>

         <!-- datasource -->
         <xsl:call-template name="datasource">
            <xsl:with-param name="data-source-name" select="$weblogic-rdbms-bean/data-source-name"/>
         </xsl:call-template>

         <!-- table-map -->
	 <xsl:choose>
	  <xsl:when test="$weblogic-rdbms-bean/table-map">
              <xsl:call-template name="table-map">
	         <xsl:with-param name="weblogic-rdbms-bean-table" select="$weblogic-rdbms-bean/table-map"/>
	      </xsl:call-template>
	  </xsl:when>
	  <xsl:otherwise>
              <xsl:call-template name="table-map">
	         <xsl:with-param name="weblogic-rdbms-bean-table" select="$weblogic-rdbms-bean"/>
	      </xsl:call-template>
	  </xsl:otherwise>
	 </xsl:choose>

      </xsl:element> <!-- entity -->
   </xsl:template> <!-- entity -->

   <xsl:template name="table-map">
	 
         <xsl:param name="weblogic-rdbms-bean-table"/>
      
         <!-- table-name -->
         <xsl:copy-of select="$weblogic-rdbms-bean-table/table-name"/>

         <!-- cmp-fields -->
         <xsl:for-each select="$weblogic-rdbms-bean-table/field-map">
            <!-- cmp-field -->
            <xsl:call-template name="cmp-field">
               <xsl:with-param name="field-map" select="."/>
            </xsl:call-template> <!-- cmp-field -->
         </xsl:for-each> <!-- field-map -->

         <!-- load-groups -->
         <xsl:if test="$weblogic-rdbms-bean-table/field-group">
            <xsl:element name="load-groups">
               <xsl:for-each select="$weblogic-rdbms-bean-table/field-group">
                  <!-- load-group -->
                  <xsl:call-template name="load-group">
                     <xsl:with-param name="field-group" select="."/>
                  </xsl:call-template> <!-- load-group -->
               </xsl:for-each> <!-- field-group -->
            </xsl:element>
         </xsl:if> <!-- load-groups -->

   </xsl:template> <!-- entity -->

   <!--
      | Template: load-group
      | Parameters:
      |    field-group - the field-group element
   -->
   <xsl:template name="load-group">
      <xsl:param name="field-group"/>

      <!-- load-group -->
      <xsl:element name="load-group">

         <!-- load-group-name -->
         <xsl:element name="load-group-name">
            <xsl:value-of select="$field-group/group-name"/>
         </xsl:element> <!-- load-group-name -->

         <xsl:for-each select="$field-group/cmp-field | $field-group/cmr-field">
            <!-- field-name -->
            <xsl:element name="field-name">
               <xsl:value-of select="."/>
            </xsl:element> <!-- field-name -->
         </xsl:for-each>

      </xsl:element> <!-- load-group -->
   </xsl:template> <!-- load-group -->

   <!--
      | Template: cmp-field 
      | Parameters:
      |   field-map - the field-map element
   -->
   <xsl:template name="cmp-field">
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
      | Template: create-table
      | Parameters:
      |   weblogic-rdbms-jar - the weblogic-rdbms-jar element
   -->
   <xsl:template name="create-table">
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
      | Template: remove-table
      | Parameters:
      |    weblogic-rdbms-jar - the weblogic-rdbms-jar element
      | Logic: remove-table is set to the value of create-default-dbms-tables
   -->
   <xsl:template name="remove-table">
      <xsl:param name="weblogic-rdbms-jar"/>

      <!-- remove-table -->
      <xsl:element name="remove-table">
         <xsl:choose>
            <xsl:when test="$remove-table='create-default-dbms-tables'">
               <xsl:choose>
                  <xsl:when test="$weblogic-rdbms-jar[(create-default-dbms-tables='true')
                                             or (create-default-dbms-tables='True')]">
                     <xsl:text>true</xsl:text>
                  </xsl:when>
                  <xsl:otherwise>
                     <xsl:text>false</xsl:text>
                  </xsl:otherwise>
               </xsl:choose>
            </xsl:when>
            <xsl:otherwise>
               <xsl:value-of select="$remove-table"/>
            </xsl:otherwise>
         </xsl:choose>
      </xsl:element> <!-- remove-table -->
   </xsl:template> <!-- remove-table -->

   <!--
      | Template: relationships
      | Parameters:
      |    weblogic-rdbms-jar - the weblogic-rdbms-jar element
   -->
   <xsl:template name="relationships">
      <xsl:param name="weblogic-rdbms-jar"/>

      <!-- relationships -->
      <xsl:element name="relationships">
         <xsl:for-each select="$weblogic-rdbms-jar/weblogic-rdbms-relation">
            <!-- ejb-relation -->
            <xsl:element name="ejb-relation">

               <!-- variable relation name -->
               <xsl:variable name="relation-name" select="./relation-name"/>

               <!-- ejb-relation-name -->
               <xsl:element name="ejb-relation-name">
                  <xsl:value-of select="$relation-name"/>
               </xsl:element>

               <!-- choose key mapping -->
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

               <!-- relationship-roles -->
               <xsl:choose>
                  <!-- relationship table mapping -->
                  <xsl:when test="count(./weblogic-relationship-role) = 2">
                     <xsl:for-each select="./weblogic-relationship-role">
                        <!-- current role name -->
                        <xsl:variable name="thisRoleName" select="./relationship-role-name"/>
                        <!-- the current side entity ejb-name -->
                        <xsl:variable name="ejb-name">
                           <xsl:for-each select="$ejb-jar/relationships/ejb-relation[ejb-relation-name=$relation-name]">
                              <xsl:for-each select="./ejb-relationship-role[ejb-relationship-role-name=$thisRoleName]">
                                 <xsl:value-of select="./relationship-role-source/ejb-name"/>
                              </xsl:for-each>
                           </xsl:for-each>
                        </xsl:variable>

                        <!-- ejb-relationship-role -->
                        <xsl:element name="ejb-relationship-role">
                           <!-- ejb-relationship-role-name -->
                           <xsl:element name="ejb-relationship-role-name">
                              <xsl:value-of select="./relationship-role-name"/>
                           </xsl:element>

                           <!-- key-fields -->
                           <xsl:element name="key-fields">
                              <xsl:for-each select="./column-map">
                                 <!-- key-field -->
                                 <xsl:call-template name="key-field">
                                    <xsl:with-param name="column-map" select="."/>
                                    <xsl:with-param name="ejb-name" select="$ejb-name"/>
                                 </xsl:call-template>
                              </xsl:for-each> <!-- column-map -->
                           </xsl:element> <!-- key-fields -->
                        </xsl:element> <!-- ejb-relationship-role -->
                     </xsl:for-each> <!-- weblogic-relationship-role -->
                  </xsl:when>

                  <!-- foreign-key mapping -->
                  <xsl:otherwise>
                     <!-- role name specified in weblogic-cmp-rdbms-jar.xsl -->
                     <xsl:variable name="thisRoleName" select="./weblogic-relationship-role/relationship-role-name"/>
                     <!-- the ejb-name of the related entity -->
                     <xsl:variable name="related-ejb-name">
                        <xsl:for-each select="$ejb-jar/relationships/ejb-relation[ejb-relation-name=$relation-name]">
                           <xsl:for-each select="./ejb-relationship-role[ejb-relationship-role-name!=$thisRoleName]">
                              <xsl:value-of select="./relationship-role-source/ejb-name"/>
                           </xsl:for-each>
                        </xsl:for-each>
                     </xsl:variable>

                     <!-- the opposite ejb-relationship-role -->
                     <xsl:element name="ejb-relationship-role">
                        <!-- find the opposite ejb-relationship-role-name in ejb-jar.xml -->
                        <xsl:for-each select="$ejb-jar/relationships/ejb-relation[ejb-relation-name=$relation-name]">
                           <xsl:for-each select="./ejb-relationship-role[ejb-relationship-role-name!=$thisRoleName]">
                              <xsl:copy-of select="./ejb-relationship-role-name"/>
                           </xsl:for-each>
                        </xsl:for-each>

                        <!-- key-fields -->
                        <xsl:element name="key-fields"/>
                     </xsl:element> <!-- ejb-relationship-role -->

                     <!-- construct this side of the relationship -->
                     <xsl:element name="ejb-relationship-role">
                        <!-- ejb-relationship-role-name -->
                        <xsl:element name="ejb-relationship-role-name">
                           <xsl:value-of select="$thisRoleName"/>
                        </xsl:element> <!-- ejb-relationship-role-name -->

                        <!-- key-fields -->
                        <xsl:element name="key-fields">
                           <xsl:for-each select="./weblogic-relationship-role/column-map">
                              <xsl:call-template name="key-field">
                                 <xsl:with-param name="column-map" select="."/>
                                 <xsl:with-param name="ejb-name" select="$related-ejb-name"/>
                              </xsl:call-template>
                           </xsl:for-each>
                        </xsl:element> <!-- key-fields -->

                     </xsl:element> <!-- ejb-relationship-role -->
                  </xsl:otherwise>
               </xsl:choose>

            </xsl:element> <!-- ejb-relation -->
         </xsl:for-each> <!-- weblogic-rdbms-relation -->
      </xsl:element> <!-- relationships -->
   </xsl:template> <!-- relationships -->

   <!--
      | Template: key-field
      | Parameters:
      |    key-field - the key-field element
   -->
   <xsl:template name="key-field">
      <xsl:param name="column-map"/>
      <xsl:param name="ejb-name"/>

      <!-- key-field -->
      <xsl:element name="key-field">

         <!-- field-name corresponding to the key-column -->
         <xsl:element name="field-name">
            <xsl:for-each select="//weblogic-rdbms-bean[ejb-name=$ejb-name]">
               <xsl:for-each select="./field-map[dbms-column=$column-map/key-column]">
                  <xsl:value-of select="./cmp-field"/>
               </xsl:for-each>
            </xsl:for-each>
         </xsl:element>

         <!-- column-name -->
         <xsl:for-each select="$column-map/foreign-key-column">
            <xsl:element name="column-name">
               <xsl:value-of select="."/>
            </xsl:element>
         </xsl:for-each>

      </xsl:element>
   </xsl:template> <!-- key-field -->

</xsl:stylesheet>
