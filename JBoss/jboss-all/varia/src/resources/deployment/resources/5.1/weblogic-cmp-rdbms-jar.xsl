<?xml version="1.0"?>
<!--
   | JBoss, the OpenSource J2EE server
   |
   | Distributable under LGPL license.
   | See terms of license at gnu.org.
-->

<!--
   | Stylesheet for weblogic-ejb-jar.xml to jboss.xml transformation.
   | WebLogic version: 5.1
   |
   | @author <a href="mailto:alex@jboss.org">Alexey Loubyansky</a>
-->
<xsl:stylesheet
   version="1.0"
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

   <!-- global parameters -->
   <xsl:param name="ejb-jar-xml">ejb-jar.xml</xsl:param>
   <xsl:param name="remote">RemoteInterface</xsl:param>

   <!-- global variables -->
   <xsl:variable name="ejb-jar" select="document($ejb-jar-xml)/ejb-jar"/>

   <!-- root template -->
   <xsl:template match="/">
      <xsl:call-template name="enterprise-beans"/>
   </xsl:template>

   <!--
      | enterprise-beans
   -->
   <xsl:template name="enterprise-beans">
      <xsl:for-each select="//weblogic-rdbms-bean">
         <xsl:call-template name="entity">
            <xsl:with-param name="weblogic-rdbms-bean" select="."/>
         </xsl:call-template>
      </xsl:for-each>
   </xsl:template>

   <!--
      | entity
   -->
   <xsl:template name="entity">
      <xsl:param name="weblogic-rdbms-bean"/>

      <!-- entity -->
      <xsl:element name="entity">

         <!-- ejb-name -->
         <xsl:for-each select="$ejb-jar/enterprise-beans/entity[remote=$remote]">
            <xsl:copy-of select="./ejb-name"/>
         </xsl:for-each>

         <!-- table-name -->
         <xsl:copy-of select="$weblogic-rdbms-bean/table-name"/>

         <!-- cmp-field* -->
         <xsl:for-each select="$weblogic-rdbms-bean/attribute-map/object-link">
            <xsl:call-template name="cmp-field">
               <xsl:with-param name="object-link" select="."/>
            </xsl:call-template>
         </xsl:for-each>
      </xsl:element>
   </xsl:template>

   <!--
      | cmp-field
   -->
   <xsl:template name="cmp-field">
      <xsl:param name="object-link"/>

      <!-- cmp-field -->
      <xsl:element name="cmp-field">
         <!-- field-name -->
         <xsl:element name="field-name">
            <xsl:value-of select="$object-link/bean-field"/>
         </xsl:element>
         <!-- column-name -->
         <xsl:element name="column-name">
            <xsl:value-of select="$object-link/dbms-column"/>
         </xsl:element>
      </xsl:element>
   </xsl:template>
</xsl:stylesheet>
