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
   <xsl:param name="default-entity-container">Standard CMP EntityBean</xsl:param>

   <!-- global variables -->
   <xsl:variable name="ejb-jar" select="document($ejb-jar-xml)/ejb-jar"/>
   <xsl:variable name="weblogic-ejb-jar" select="//weblogic-ejb-jar"/>

   <!-- Root template -->
   <xsl:template match="/">
      <xsl:element name="jboss">

         <!-- enterprise-beans -->
         <xsl:call-template name="enterprise-beans"/>

         <!-- container-configurations -->
         <xsl:call-template name="container-configurations"/>

      </xsl:element> <!-- jboss -->
   </xsl:template>

   <!--
      | enterprise-beans
   -->
   <xsl:template name="enterprise-beans">
      <xsl:element name="enterprise-beans">

         <!-- session* -->
         <xsl:for-each select="$ejb-jar/enterprise-beans/session">
            <xsl:variable name="ejb-name" select="./ejb-name"/>
            <xsl:variable name="weblogic-enterprise-bean" select="$weblogic-ejb-jar/weblogic-enterprise-bean[ejb-name=$ejb-name]"/>
            <xsl:if test="$weblogic-enterprise-bean">
               <xsl:call-template name="session">
                  <xsl:with-param name="ejb-jar-session" select="."/>
                  <xsl:with-param name="weblogic-enterprise-bean" select="$weblogic-enterprise-bean"/>
               </xsl:call-template>
            </xsl:if>
         </xsl:for-each>

         <!-- entity* -->
         <xsl:for-each select="$ejb-jar/enterprise-beans/entity">
            <xsl:variable name="ejb-name" select="./ejb-name"/>
            <xsl:variable name="weblogic-enterprise-bean" select="$weblogic-ejb-jar/weblogic-enterprise-bean[ejb-name=$ejb-name]"/>
            <xsl:if test="$weblogic-enterprise-bean">
               <xsl:call-template name="entity">
                  <xsl:with-param name="ejb-jar-entity" select="."/>
                  <xsl:with-param name="weblogic-enterprise-bean" select="$weblogic-enterprise-bean"/>
               </xsl:call-template>
            </xsl:if>
         </xsl:for-each>

      </xsl:element> <!-- enterprise-beans -->
   </xsl:template> <!-- eneterprise-beans -->

   <!--
      | session
   -->
   <xsl:template name="session">
      <xsl:param name="ejb-jar-session"/>
      <xsl:param name="weblogic-enterprise-bean"/>

      <xsl:variable name="ejb-name" select="$ejb-jar-session/ejb-name"/>

      <!-- session -->
      <xsl:element name="session">

         <!-- ejb-name -->
         <xsl:copy-of select="$ejb-jar-session/ejb-name"/>

         <!-- jndi-name -->
         <xsl:if test="$weblogic-enterprise-bean/jndi-name">
            <xsl:copy-of select="$weblogic-enterprise-bean/jndi-name"/>
         </xsl:if>

         <!-- ejb-ref* -->
         <xsl:for-each select="$weblogic-enterprise-bean/reference-descriptor/ejb-reference-description">
            <xsl:call-template name="ejb-ref">
               <xsl:with-param name="ejb-reference-description" select="."/>
            </xsl:call-template>
         </xsl:for-each>

         <!-- resource-ref* -->
         <xsl:for-each select="$weblogic-enterprise-bean/reference-descriptor/resource-description">
            <xsl:call-template name="resource-ref">
               <xsl:with-param name="resource-description" select="."/>
            </xsl:call-template>
         </xsl:for-each>

         <!-- clustered -->
         <xsl:if test="$weblogic-enterprise-bean/clustering-descriptor">
            <xsl:element name="clustered"/>
         </xsl:if>
      </xsl:element>
   </xsl:template>

   <!--
      | entity
   -->
   <xsl:template name="entity">
      <xsl:param name="ejb-jar-entity"/>
      <xsl:param name="weblogic-enterprise-bean"/>

      <xsl:variable name="ejb-name" select="$ejb-jar-entity/ejb-name"/>

      <!-- entity -->
      <xsl:element name="entity">

         <!-- ejb-name -->
         <xsl:copy-of select="$ejb-jar-entity/ejb-name"/>

         <!-- jndi-name -->
         <xsl:if test="$weblogic-enterprise-bean/jndi-name">
            <xsl:copy-of select="$weblogic-enterprise-bean/jndi-name"/>
         </xsl:if>

         <!-- read-only -->
         <xsl:variable name="cache-strategy" select="$weblogic-enterprise-bean/caching-descriptor/cache-strategy"/>
         <xsl:if test="$cache-strategy = 'Read-Only'">
            <xsl:element name="read-only"/>
         </xsl:if>

         <!-- configuration-name -->
         <xsl:element name="configuration-name">
            <xsl:call-template name="generate-container-name">
               <xsl:with-param name="ejb-name" select="$ejb-name"/>
            </xsl:call-template>
         </xsl:element>

         <!-- ejb-ref* -->
         <xsl:for-each select="$weblogic-enterprise-bean/reference-descriptor/ejb-reference-description">
            <xsl:call-template name="ejb-ref">
               <xsl:with-param name="ejb-reference-description" select="."/>
            </xsl:call-template>
         </xsl:for-each>

         <!-- resource-ref* -->
         <xsl:for-each select="$weblogic-enterprise-bean/reference-descriptor/resource-description">
            <xsl:call-template name="resource-ref">
               <xsl:with-param name="resource-description" select="."/>
            </xsl:call-template>
         </xsl:for-each>

         <!-- clustered -->
         <xsl:if test="$weblogic-enterprise-bean/clustering-descriptor">
            <xsl:element name="clustered"/>
         </xsl:if>
      </xsl:element>
   </xsl:template>

   <!--
      | ejb-ref
   -->
   <xsl:template name="ejb-ref">
      <xsl:param name="ejb-reference-description"/>

      <xsl:element name="ejb-ref">
         <xsl:copy-of select="$ejb-reference-description/ejb-ref-name"/>
         <xsl:copy-of select="$ejb-reference-description/jndi-name"/>
      </xsl:element>
   </xsl:template>

   <!--
      | resource-ref
   -->
   <xsl:template name="resource-ref">
      <xsl:param name="resource-description"/>

      <xsl:element name="resource-ref">
         <xsl:copy-of select="$resource-description/res-ref-name"/>
         <xsl:copy-of select="$resource-description/jndi-name"/>
      </xsl:element>
   </xsl:template>

   <!--
      | container-configurations
   -->
   <xsl:template name="container-configurations">
      <xsl:element name="container-configurations">

         <xsl:for-each select="$ejb-jar/enterprise-beans/entity">
            <xsl:variable name="ejb-name" select="./ejb-name"/>
            <xsl:variable name="weblogic-enterprise-bean" select="$weblogic-ejb-jar/weblogic-enterprise-bean[ejb-name=$ejb-name]"/>
            <xsl:if test="$weblogic-enterprise-bean">
               <xsl:call-template name="container-configuration">
                  <xsl:with-param name="weblogic-enterprise-bean" select="$weblogic-enterprise-bean"/>
                  <xsl:with-param name="extends" select="$default-entity-container"/>
               </xsl:call-template>
            </xsl:if>
         </xsl:for-each>

      </xsl:element>
   </xsl:template>

   <!--
      | container-configuration
   -->
   <xsl:template name="container-configuration">
      <xsl:param name="weblogic-enterprise-bean"/>
      <xsl:param name="extends"/>

      <!-- container-configuration -->
      <xsl:element name="container-configuration">
         <xsl:attribute name="extends">
            <xsl:value-of select="$extends"/>
         </xsl:attribute>

         <!-- container-name -->
         <xsl:element name="container-name">
            <xsl:call-template name="generate-container-name">
               <xsl:with-param name="ejb-name" select="$weblogic-enterprise-bean/ejb-name"/>
            </xsl:call-template>
         </xsl:element>

         <!-- persistence-manager -->
         <xsl:element name="persistence-manager">
            <xsl:text>org.jboss.ejb.plugins.cmp.jdbc.JDBCStoreManager</xsl:text>
         </xsl:element>

         <xsl:variable name="db-is-shared" select="$weblogic-enterprise-bean/persistence-descriptor/db-is-shared"/>
         <xsl:if test="$db-is-shared = 'True'">
            <xsl:element name="commit-option">
               <xsl:text>A</xsl:text>
            </xsl:element>
         </xsl:if>
      </xsl:element>
   </xsl:template>

   <!--
      | Generates container name
   -->
   <xsl:template name="generate-container-name">
      <xsl:param name="ejb-name"/>
      <xsl:value-of select="concat($ejb-name, ' Container Configuration')"/>
   </xsl:template>
</xsl:stylesheet>
