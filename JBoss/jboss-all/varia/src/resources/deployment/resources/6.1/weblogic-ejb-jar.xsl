<?xml version="1.0"?>
<!--
   | JBoss, the OpenSource EJB server
   |
   | Distributable under LGPL license.
   | See terms of license at gnu.org.
-->

<!--
   | Stylesheet for weblogic-ejb-jar.xml to jboss.xml transformation.
   | WebLogic version: 6.1
   |
   | @author <a href="mailto:alex@jboss.org">Alex Loubyansky</a>
-->
<xsl:stylesheet
   version="1.0"
   xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

   <!-- global parameters -->
   <xsl:param name="ejb-jar-xml">ejb-jar.xml</xsl:param>
   <xsl:param name="default-entity-container">Standard CMP 2.x EntityBean</xsl:param>

   <!-- global variables -->
   <xsl:variable name="ejb-jar" select="document($ejb-jar-xml)/ejb-jar"/>
   <xsl:variable name="weblogic-ejb-jar" select="//weblogic-ejb-jar"/>

   <!-- root -->
   <xsl:template match="/">
      <!-- jboss -->
      <xsl:element name="jboss">
         <!-- generate enterprise-beans -->
         <xsl:call-template name="enterprise-beans"/>
         <!-- generate container-configurations -->
         <xsl:call-template name="container-configurations"/>
      </xsl:element>
   </xsl:template>

   <!--
      | enterprise-beans template
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

         <!-- message-driven* -->
         <xsl:for-each select="$ejb-jar/enterprise-beans/message-driven">
            <xsl:variable name="ejb-name" select="./ejb-name"/>
            <xsl:variable name="weblogic-enterprise-bean" select="$weblogic-ejb-jar/weblogic-enterprise-bean[ejb-name=$ejb-name]"/>
            <xsl:if test="$weblogic-enterprise-bean">
               <xsl:call-template name="message-driven">
                  <xsl:with-param name="ejb-jar-entity" select="."/>
                  <xsl:with-param name="weblogic-enterprise-bean" select="$weblogic-enterprise-bean"/>
               </xsl:call-template>
            </xsl:if>
         </xsl:for-each>

      </xsl:element>
   </xsl:template> <!-- eneterprise-beans -->

   <!--
      | message-driven
   -->
   <xsl:template name="message-driven">
      <xsl:param name="ejb-jar-entity"/>
      <xsl:param name="weblogic-enterprise-bean"/>

      <xsl:variable name="ejb-name" select="$ejb-jar-entity/ejb-name"/>

      <!-- message-driven -->
      <xsl:element name="message-driven">

         <!-- ejb-name -->
         <xsl:copy-of select="$ejb-jar-entity/ejb-name"/>

         <!-- destination-jndi-name -->
         <xsl:copy-of select="$weblogic-enterprise-bean/message-driven-descriptor/destination-jndi-name"/>

         <!-- mdb-client-id -->
         <!-- WL uses ejb-name as default client id -->
         <xsl:variable name="jms-client-id" select="$weblogic-enterprise-bean/message-driven-descriptor/jms-client-id"/>
         <xsl:if test="$jms-client-id">
            <xsl:element name="mdb-client-id">
               <xsl:value-of select="$jms-client-id"/>
            </xsl:element>
         </xsl:if>

         <!-- ejb-ref, ejb-local-ref, resource-ref, resource-env-ref -->
         <xsl:if test="$weblogic-enterprise-bean/reference-descriptor">
            <xsl:call-template name="reference-descriptor">
               <xsl:with-param name="reference-descriptor" select="$weblogic-enterprise-bean/reference-descriptor"/>
            </xsl:call-template>
         </xsl:if>

      </xsl:element>
   </xsl:template> <!-- message-driven -->

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

         <!-- local-jndi-name -->
         <xsl:if test="$weblogic-enterprise-bean/local-jndi-name">
            <xsl:copy-of select="$weblogic-enterprise-bean/local-jndi-name"/>
         </xsl:if>

         <!-- read-only -->
         <xsl:variable name="ReadOnly" select="$weblogic-enterprise-bean/entity-descriptor/entity-cache[concurrency-strategy='ReadOnly']"/>
         <xsl:if test="$ReadOnly">
            <xsl:element name="read-only">
               <xsl:text>true</xsl:text>
            </xsl:element>
         </xsl:if>

         <!-- configuration-name -->
         <xsl:element name="configuration-name">
            <xsl:call-template name="generate-container-name">
               <xsl:with-param name="ejb-name" select="$ejb-name"/>
            </xsl:call-template>
         </xsl:element>

         <!-- ejb-ref, ejb-local-ref, resource-ref, resource-env-ref -->
         <xsl:if test="$weblogic-enterprise-bean/reference-descriptor">
            <xsl:call-template name="reference-descriptor">
               <xsl:with-param name="reference-descriptor" select="$weblogic-enterprise-bean/reference-descriptor"/>
            </xsl:call-template>
         </xsl:if>

         <!--clustered -->
         <xsl:if test="$weblogic-enterprise-bean/entity-descriptor/entity-clustering">
            <xsl:element name="clustered">
               <xsl:text>true</xsl:text>
            </xsl:element>
         </xsl:if>

      </xsl:element>
   </xsl:template> <!-- entity -->

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

         <!-- local-jndi-name -->
         <xsl:if test="$weblogic-enterprise-bean/local-jndi-name">
            <xsl:copy-of select="$weblogic-enterprise-bean/local-jndi-name"/>
         </xsl:if>

         <!-- ejb-ref, ejb-local-ref, resource-ref, resource-env-ref -->
         <xsl:if test="$weblogic-enterprise-bean/reference-descriptor">
            <xsl:call-template name="reference-descriptor">
               <xsl:with-param name="reference-descriptor" select="$weblogic-enterprise-bean/reference-descriptor"/>
            </xsl:call-template>
         </xsl:if>

         <!--clustered -->
         <xsl:if test="$weblogic-enterprise-bean/stateless-session-descriptor/stateless-clustering">
            <xsl:element name="clustered">
               <xsl:text>true</xsl:text>
            </xsl:element>
         </xsl:if>
         <xsl:if test="$weblogic-enterprise-bean/stateful-session-descriptor/stateful-session-clustering">
            <xsl:element name="clustered">
               <xsl:text>true</xsl:text>
            </xsl:element>
         </xsl:if>
      </xsl:element>
   </xsl:template> <!-- session -->

   <!--
      | transforms reference-descriptor to ejb-ref, ejb-local-ref, resource-ref, resource-env-ref
   -->
   <xsl:template name="reference-descriptor">
      <xsl:param name="reference-descriptor"/>

      <!-- ejb-ref* -->
      <xsl:for-each select="$reference-descriptor/ejb-reference-description">
         <xsl:element name="ejb-ref">
            <xsl:copy-of select="./ejb-ref-name"/>
            <xsl:copy-of select="./jndi-name"/>
         </xsl:element>
      </xsl:for-each>

      <!-- ejb-local-ref* -->
      <xsl:for-each select="$reference-descriptor/ejb-local-reference-description">
         <xsl:element name="ejb-local-ref">
            <xsl:copy-of select="./ejb-ref-name"/>
            <xsl:element name="local-jndi-name">
               <xsl:value-of select="./jndi-name"/>
            </xsl:element>
         </xsl:element>
      </xsl:for-each>

      <!-- resource-ref* -->
      <xsl:for-each select="$reference-descriptor/resource-description">
         <xsl:element name="resource-ref">
            <xsl:copy-of select="./res-ref-name"/>
            <xsl:copy-of select="./jndi-name"/>
         </xsl:element>
      </xsl:for-each>

      <!-- resource-env-ref* -->
      <xsl:for-each select="$reference-descriptor/resource-env-description">
         <xsl:element name="resource-env-ref">
            <xsl:element name="resource-env-ref-name">
               <xsl:value-of select="./res-env-ref-name"/>
            </xsl:element>
            <xsl:copy-of select="./jndi-name"/>
         </xsl:element>
      </xsl:for-each>
   </xsl:template> <!-- reference-descriptor -->

   <!--
      | container-configurations
   -->
   <xsl:template name="container-configurations">
      <xsl:element name="container-configurations">

         <xsl:for-each select="$ejb-jar/enterprise-beans/entity">
            <xsl:variable name="ejb-name" select="./ejb-name"/>
            <xsl:variable name="weblogic-enterprise-bean" select="$weblogic-ejb-jar/weblogic-enterprise-bean[ejb-name=$ejb-name]"/>
            <xsl:if test="$weblogic-enterprise-bean">
               <xsl:call-template name="entity-container-configuration">
                  <xsl:with-param name="ejb-jar-entity" select="."/>
                  <xsl:with-param name="weblogic-enterprise-bean" select="$weblogic-enterprise-bean"/>
               </xsl:call-template>
            </xsl:if>
         </xsl:for-each>

      </xsl:element>
   </xsl:template> <!-- conatiner-configurations -->

   <!--
      | entity container-configuration
   -->
   <xsl:template name="entity-container-configuration">
      <xsl:param name="ejb-jar-entity"/>
      <xsl:param name="weblogic-enterprise-bean"/>

      <xsl:variable name="ejb-name" select="$ejb-jar-entity/ejb-name"/>
      <xsl:variable name="entity-descriptor" select="$weblogic-enterprise-bean/entity-descriptor"/>

      <!-- container-configuration -->
      <xsl:element name="container-configuration">
         <xsl:attribute name="extends">
            <xsl:value-of select="$default-entity-container"/>
         </xsl:attribute>

         <!-- container-name -->
         <xsl:element name="container-name">
            <xsl:call-template name="generate-container-name">
               <xsl:with-param name="ejb-name" select="$ejb-name"/>
            </xsl:call-template>
         </xsl:element>

         <!-- commit-option -->
         <xsl:choose>
            <xsl:when test="$entity-descriptor/entity-cache/concurrency-strategy[text()='ReadOnly']">
               <xsl:choose>
                  <xsl:when test="$entity-descriptor/entity-cache[read-timeout-seconds>0]">
                     <xsl:element name="commit-option">
                        <xsl:text>D</xsl:text>
                     </xsl:element>
                     <xsl:element name="optiond-refresh-rate">
                        <xsl:value-of select="$entity-descriptor/entity-cache/read-timeout-seconds"/>
                     </xsl:element>
                  </xsl:when>
                  <xsl:otherwise>
                     <xsl:element name="commit-option">
                        <xsl:text>A</xsl:text>
                     </xsl:element>
                  </xsl:otherwise>
               </xsl:choose>
            </xsl:when>

            <xsl:when test="$entity-descriptor/persistence[db-is-shared='false']">
               <xsl:element name="commit-option">
                  <xsl:text>A</xsl:text>
               </xsl:element>
            </xsl:when>

            <!-- for 7.0 -->
            <xsl:when test="$entity-descriptor/persistence[cache-between-transactions='true']">
               <xsl:element name="commit-option">
                  <xsl:text>A</xsl:text>
               </xsl:element>
            </xsl:when>
         </xsl:choose>
      </xsl:element>

   </xsl:template> <!-- entity-container-configurations -->

   <!--
      | Generates container name
   -->
   <xsl:template name="generate-container-name">
      <xsl:param name="ejb-name"/>
      <xsl:value-of select="concat($ejb-name, ' Container Configuration')"/>
   </xsl:template>

</xsl:stylesheet>
