<?xml version="1.0"?>

<!--
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
-->

<!--
* Stylesheet for weblogic-ejb-jar.xml to jboss.xml transformation.
*
* @author <a href="mailto:loubyansky@ukr.net">Alex Loubyansky</a>
-->

<!--
ToDo:
- by default wl's beans are clusterable. should i use clusterable container
  configurations by default in jboss?
- transaction-isolation. In JBoss configured in datasource xxx-service.xml
  file, AFAIK.
- finders-load-bean. if set to true, should be mapped to load-group and
  query with read-ahead startegy on-find in jbosscmp-jdbc.xml.
  This is NOT IMPLEMENTED YET.
- connection-factory-jndi-name for MDB in WL. This should be configured in
  jbossmq-service.xml
  This is NOT IMPLEMENTED YET.
- resource-ref vs resource-env-ref
- security
-->

<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

   <!-- Global parameters -->
   <xsl:param name="standardwl">standardwl.xml</xsl:param>

   <!-- Global variables -->
   <xsl:variable name="StandardCMPEntity2X">Standard CMP 2.x EntityBean</xsl:variable>
   <xsl:variable name="StandardStatelessSessionBean">Standard Stateless SessionBean</xsl:variable>
   <xsl:variable name="StandardStatefulSessionBean">Standard Stateful SessionBean</xsl:variable>
   <xsl:variable name="ClusteredStatefulSessionBean">Clustered Stateful SessionBean</xsl:variable>
   <xsl:variable name="StandardMessageDrivenBean">Standard Message Driven Bean</xsl:variable>
                                                   
   <!-- Root template -->
   <xsl:template match="/">
      <xsl:element name="jboss">

         <!-- generate enterprise-beans -->
         <xsl:call-template name="enterprise-beans"/>

         <!-- generate container-configurations -->
         <xsl:call-template name="container-configurations"/>

      </xsl:element> <!-- jboss -->
   </xsl:template>


   <!-- ======================================================= -->
   <!-- template for enterprise-beans                           -->
   <!-- ======================================================= -->
   <xsl:template name="enterprise-beans">
         <xsl:element name="enterprise-beans">

            <!-- generate entity beans -->
            <xsl:for-each select="//weblogic-enterprise-bean/entity-descriptor">
               <xsl:call-template name="entity">
                  <xsl:with-param name="entity-descriptor" select="."/>
               </xsl:call-template> <!-- entity -->
            </xsl:for-each> <!-- entity-descriptor -->

            <!-- generate stateless session beans -->
            <xsl:for-each select="//weblogic-enterprise-bean/stateless-session-descriptor">
               <xsl:call-template name="stateless-session">
                  <xsl:with-param name="stateless-session-descriptor" select="."/>
               </xsl:call-template> <!-- stateless-session -->
            </xsl:for-each> <!-- stateless-session-descriptor -->

            <!-- generate stateful session beans -->
            <xsl:for-each select="//weblogic-enterprise-bean/stateful-session-descriptor">
               <xsl:call-template name="stateful-session">
                  <xsl:with-param name="stateful-session-descriptor" select="."/>
               </xsl:call-template> <!-- stateful-session -->
            </xsl:for-each> <!-- stateful-session-descriptor -->

            <!-- generate message driven beans -->
            <xsl:for-each select="//weblogic-enterprise-bean/message-driven-descriptor">
               <xsl:call-template name="message-driven">
                  <xsl:with-param name="message-driven-descriptor" select="."/>
               </xsl:call-template> <!-- message-driven -->
            </xsl:for-each> <!-- message-driven-descriptor -->

         </xsl:element> <!-- enterprise-beans -->
   </xsl:template> <!-- eneterprise-beans -->


   <!-- ======================================================= -->
   <!-- template for message-driven beans                       -->
   <!-- ======================================================= -->
   <xsl:template name="message-driven">
      <!-- template parameters -->
      <xsl:param name="message-driven-descriptor"/>

      <!-- pointer to weblogic-enterprise-bean -->
      <xsl:variable name="weblogic-enterprise-bean"
                    select="$message-driven-descriptor/.."/>

      <xsl:element name="message-driven">

         <!-- construct configuration-name -->
         <xsl:element name="configuration-name">
            <xsl:value-of select="concat($weblogic-enterprise-bean/ejb-name, ' Container Configuration')"/>
         </xsl:element> <!-- configuration-name -->

         <!-- copy ejb-name -->
         <xsl:call-template name="ejb-name">
            <xsl:with-param name="weblogic-enterprise-bean" select="$weblogic-enterprise-bean"/>
         </xsl:call-template> <!-- ejb-name -->

         <!-- destination-jndi-name -->
         <xsl:copy-of select="$message-driven-descriptor/destination-jndi-name"/>

         <!-- mdb-client-id -->
         <!-- WL uses ejb-name as default client id -->
         <!-- <xsl:element name="mdb-client-id">
            <xsl:choose>
               <xsl:when test="$message-driven-descriptor[jms-client-id]">
                  <xsl:value-of select="$message-driven-descriptor/jms-client-id"/>
               </xsl:when>
               <xsl:otherwise>
                  <xsl:value-of select="$weblogic-enterprise-bean/ejb-name"/>
               </xsl:otherwise>
            </xsl:choose>
         </xsl:element> --> <!-- mdb-client-id -->

         <!-- generate ejb-ref -->
         <xsl:call-template name="ejb-ref">
            <xsl:with-param name="reference-descriptor"
                            select="$weblogic-enterprise-bean/reference-descriptor"/>
         </xsl:call-template> <!-- ejb-ref -->

         <!-- generate resource-ref -->
         <xsl:call-template name="resource-ref">
            <xsl:with-param name="reference-descriptor"
                            select="$weblogic-enterprise-bean/reference-descriptor"/>
         </xsl:call-template> <!-- resource-ref -->

         <!-- generate resource-env-ref -->
         <xsl:call-template name="resource-env-ref">
            <xsl:with-param name="reference-descriptor"
                            select="$weblogic-enterprise-bean/reference-descriptor"/>
         </xsl:call-template> <!-- resource-env-ref -->

      </xsl:element> <!-- message-driven -->
   </xsl:template> <!-- message-driven -->


   <!-- ======================================================= -->
   <!-- template for stateless-session beans                    -->
   <!-- ======================================================= -->
   <xsl:template name="stateless-session">
      <!-- template parameters -->
      <xsl:param name="stateless-session-descriptor"/>

      <xsl:element name="session">

         <!-- construct configuration-name -->
         <xsl:element name="configuration-name">
            <xsl:value-of select="concat($stateless-session-descriptor/../ejb-name, ' Container Configuration')"/>
         </xsl:element> <!-- configuration-name -->

         <!-- copy ejb-name -->
         <xsl:call-template name="ejb-name">
            <xsl:with-param name="weblogic-enterprise-bean" select="$stateless-session-descriptor/.."/>
         </xsl:call-template> <!-- ejb-name -->

         <!-- copy jndi-name -->
         <xsl:call-template name="jndi-name">
            <xsl:with-param name="weblogic-enterprise-bean" select="$stateless-session-descriptor/.."/>
         </xsl:call-template> <!-- jndi-name -->

         <!-- copy local-jndi-name -->
         <xsl:call-template name="local-jndi-name">
            <xsl:with-param name="weblogic-enterprise-bean" select="$stateless-session-descriptor/.."/>
         </xsl:call-template> <!-- local-name -->

         <!-- set clustered -->
         <xsl:element name="clustered">
            <xsl:value-of select="$stateless-session-descriptor/stateless-clustering/stateless-bean-is-clusterable"/>
         </xsl:element> <!-- clustered -->

         <!-- generate ejb-ref -->
         <xsl:call-template name="ejb-ref">
            <xsl:with-param name="reference-descriptor"
                            select="$stateless-session-descriptor/../reference-descriptor"/>
         </xsl:call-template> <!-- ejb-ref -->

         <!-- generate resource-ref -->
         <xsl:call-template name="resource-ref">
            <xsl:with-param name="reference-descriptor"
                            select="$stateless-session-descriptor/../reference-descriptor"/>
         </xsl:call-template> <!-- resource-ref -->

         <!-- generate resource-env-ref -->
         <xsl:call-template name="resource-env-ref">
            <xsl:with-param name="reference-descriptor"
                            select="$stateless-session-descriptor/../reference-descriptor"/>
         </xsl:call-template> <!-- resource-env-ref -->

      </xsl:element> <!-- stateless-session -->
   </xsl:template> <!-- stateless-session -->


   <!-- ======================================================= -->
   <!-- template for stateful-session beans                     -->
   <!-- ======================================================= -->
   <xsl:template name="stateful-session">
      <!-- template parameters -->
      <xsl:param name="stateful-session-descriptor"/>

      <xsl:element name="session">

         <!-- construct configuration-name -->
         <xsl:element name="configuration-name">
            <xsl:value-of select="concat($stateful-session-descriptor/../ejb-name, ' Container Configuration')"/>
         </xsl:element> <!-- configuration-name -->

         <!-- copy ejb-name -->
         <xsl:call-template name="ejb-name">
            <xsl:with-param name="weblogic-enterprise-bean" select="$stateful-session-descriptor/.."/>
         </xsl:call-template> <!-- ejb-name -->

         <!-- copy jndi-name -->
         <xsl:call-template name="jndi-name">
            <xsl:with-param name="weblogic-enterprise-bean" select="$stateful-session-descriptor/.."/>
         </xsl:call-template> <!-- jndi-name -->

         <!-- copy local-jndi-name -->
         <xsl:call-template name="local-jndi-name">
            <xsl:with-param name="weblogic-enterprise-bean" select="$stateful-session-descriptor/.."/>
         </xsl:call-template> <!-- local-jndi-name -->

         <!-- set clustered -->
         <xsl:element name="clustered">
            <xsl:value-of select="$stateful-session-descriptor/stateful-clustering/home-is-clusterable"/>
         </xsl:element> <!-- clustered -->

         <!-- generate ejb-ref -->
         <xsl:call-template name="ejb-ref">
            <xsl:with-param name="reference-descriptor"
                            select="$stateful-session-descriptor/../reference-descriptor"/>
         </xsl:call-template> <!-- ejb-ref -->

         <!-- generate resource-ref -->
         <xsl:call-template name="resource-ref">
            <xsl:with-param name="reference-descriptor"
                            select="$stateful-session-descriptor/../reference-descriptor"/>
         </xsl:call-template> <!-- resource-ref -->

         <!-- generate resource-env-ref -->
         <xsl:call-template name="resource-env-ref">
            <xsl:with-param name="reference-descriptor"
                            select="$stateful-session-descriptor/../reference-descriptor"/>
         </xsl:call-template> <!-- resource-env-ref -->

      </xsl:element> <!-- stateful-session -->
   </xsl:template> <!-- stateful-session -->


   <!-- ======================================================= -->
   <!-- template for entity                                     -->
   <!-- ======================================================= -->
   <xsl:template name="entity">
      <!-- template parameters -->
      <xsl:param name="entity-descriptor"/>

      <xsl:element name="entity">

         <!-- construct configuration-name -->
         <xsl:element name="configuration-name">
            <xsl:value-of select="concat($entity-descriptor/../ejb-name, ' Container Configuration')"/>
         </xsl:element> <!-- configuration-name -->

         <!-- copy ejb-name -->
         <xsl:call-template name="ejb-name">
            <xsl:with-param name="weblogic-enterprise-bean" select="$entity-descriptor/.."/>
         </xsl:call-template> <!-- ejb-name -->

         <!-- copy jndi-name -->
         <xsl:call-template name="jndi-name">
            <xsl:with-param name="weblogic-enterprise-bean" select="$entity-descriptor/.."/>
         </xsl:call-template> <!-- jndi-name -->

         <!-- copy local-jndi-name -->
         <xsl:call-template name="local-jndi-name">
            <xsl:with-param name="weblogic-enterprise-bean" select="$entity-descriptor/.."/>
         </xsl:call-template> <!-- local-name -->

         <!-- set read-only -->
         <xsl:element name="read-only">
            <xsl:choose>
               <xsl:when test="$entity-descriptor/entity-cache[concurrency-strategy='ReadOnly']">
                  <xsl:text>true</xsl:text>
               </xsl:when>
               <xsl:otherwise>
                  <xsl:text>false</xsl:text>
               </xsl:otherwise>
            </xsl:choose>
         </xsl:element> <!-- read-only -->

         <!-- set clustered -->
         <xsl:element name="clustered">
            <xsl:value-of select="$entity-descriptor/entity-clustering/home-is-clusterable"/>
         </xsl:element> <!-- clustered -->

         <!-- generate ejb-ref -->
         <xsl:call-template name="ejb-ref">
            <xsl:with-param name="reference-descriptor"
                            select="$entity-descriptor/../reference-descriptor"/>
         </xsl:call-template> <!-- ejb-ref -->

         <!-- generate resource-ref -->
         <xsl:call-template name="resource-ref">
            <xsl:with-param name="reference-descriptor"
                            select="$entity-descriptor/../reference-descriptor"/>
         </xsl:call-template> <!-- resource-ref -->

         <!-- generate resource-env-ref -->
         <xsl:call-template name="resource-env-ref">
            <xsl:with-param name="reference-descriptor"
                            select="$entity-descriptor/../reference-descriptor"/>
         </xsl:call-template> <!-- resource-env-ref -->

      </xsl:element> <!-- entity -->
   </xsl:template> <!-- entity -->


   <!-- ============================================ -->
   <!-- template for ejb-ref                         -->
   <!-- ============================================ -->
   <xsl:template name="ejb-ref">
      <!-- template parameters -->
      <xsl:param name="reference-descriptor"/>

      <xsl:for-each select="$reference-descriptor/ejb-reference-description">
         <xsl:element name="ejb-ref">
            <xsl:copy-of select="ejb-ref-name"/>
            <xsl:copy-of select="jndi-name"/>
         </xsl:element> <!-- ejb-ref -->
      </xsl:for-each> <!-- ejb-reference-description -->

      <xsl:for-each select="$reference-descriptor/ejb-local-reference-description">
         <xsl:element name="ejb-ref">
            <xsl:copy-of select="ejb-ref-name"/>
            <xsl:copy-of select="jndi-name"/>
         </xsl:element> <!-- ejb-ref -->
      </xsl:for-each> <!-- ejb-local-reference-description -->
   </xsl:template> <!-- ejb-ref -->


   <!-- =============================================== -->
   <!-- template for resource-ref                       -->
   <!-- =============================================== -->
   <xsl:template name="resource-ref">
      <!-- template parameters -->
      <xsl:param name="reference-descriptor"/>

      <xsl:for-each select="$reference-descriptor/resource-description">
         <xsl:element name="resource-ref">
            <xsl:copy-of select="./res-ref-name"/>
            <xsl:copy-of select="./jndi-name"/>
         </xsl:element> <!-- resource-ref -->
      </xsl:for-each>
   </xsl:template> <!-- resource-ref -->


   <!-- =============================================== -->
   <!-- template for resource-env-ref                   -->
   <!-- =============================================== -->
   <xsl:template name="resource-env-ref">
      <!-- template parameters -->
      <xsl:param name="reference-descriptor"/>

      <xsl:for-each select="$reference-descriptor/resource-env-description">
         <xsl:element name="resource-env-ref">
            <xsl:element name="resource-env-ref-name">
               <xsl:value-of select="./res-env-ref-name"/>
            </xsl:element> <!-- resource-env-ref-name -->
            <xsl:copy-of select="./jndi-name"/>
         </xsl:element> <!-- resource-env-ref -->
      </xsl:for-each>
   </xsl:template> <!-- resource-env-ref -->


   <!-- ================================================== -->
   <!-- template for ejb-name                              -->
   <!-- ================================================== -->
   <xsl:template name="ejb-name">
      <!-- template parameters -->
      <xsl:param name="weblogic-enterprise-bean"/>

      <xsl:copy-of select="$weblogic-enterprise-bean/ejb-name"/>
   </xsl:template> <!-- ejb-name -->


   <!-- ================================================== -->
   <!-- template for jndi-name                             -->
   <!-- ================================================== -->
   <xsl:template name="jndi-name">
      <!-- template parameters -->
      <xsl:param name="weblogic-enterprise-bean"/>

      <xsl:copy-of select="$weblogic-enterprise-bean/jndi-name"/>
   </xsl:template> <!-- jndi-name -->


   <!-- ================================================== -->
   <!-- template for local-jndi-name                       -->
   <!-- ================================================== -->
   <xsl:template name="local-jndi-name">
      <!-- template parameters -->
      <xsl:param name="weblogic-enterprise-bean"/>

      <xsl:copy-of select="$weblogic-enterprise-bean/local-jndi-name"/>
   </xsl:template> <!-- local-jndi-name -->


   <!-- ================================================== -->
   <!-- template for container-configurations              -->
   <!-- ================================================== -->
   <xsl:template name="container-configurations">
      <xsl:element name="container-configurations">

         <!-- loop through all entity-descriptors -->
         <xsl:for-each select="//weblogic-enterprise-bean/entity-descriptor">
            <!-- There is no Clustered CMP EntityBean configuration for now -->
            <xsl:call-template name="entity-container-configuration">
               <xsl:with-param name="wl-ejb-descriptor" select="."/>
               <xsl:with-param name="default-configuration" select="$StandardCMPEntity2X"/>
            </xsl:call-template>
         </xsl:for-each>

         <!-- loop through all stateless-session-descriptors -->
         <xsl:for-each select="//weblogic-enterprise-bean/stateless-session-descriptor">
            <!-- There is no Clustered Stateless SessionBean configuration for now -->
            <xsl:call-template name="stateless-session-container-configuration">
               <xsl:with-param name="wl-ejb-descriptor" select="."/>
               <xsl:with-param name="default-configuration" select="$StandardStatelessSessionBean"/>
            </xsl:call-template>
         </xsl:for-each>

         <!-- loop through all stateful-session-descriptors -->
         <xsl:for-each select="//weblogic-enterprise-bean/stateful-session-descriptor">
            <xsl:choose>
               <xsl:when test="./stateful-session-clustering/home-is-clusterable[text()='true']">
                  <xsl:call-template name="stateful-session-container-configuration">
                     <xsl:with-param name="wl-ejb-descriptor" select="."/>
                     <xsl:with-param name="default-configuration" select="$ClusteredStatefulSessionBean"/>
                  </xsl:call-template>
               </xsl:when>
               <xsl:otherwise>
                  <xsl:call-template name="stateful-session-container-configuration">
                     <xsl:with-param name="wl-ejb-descriptor" select="."/>
                     <xsl:with-param name="default-configuration" select="$StandardStatefulSessionBean"/>
                  </xsl:call-template>
               </xsl:otherwise>
            </xsl:choose>
         </xsl:for-each>

         <!-- loop through all message-driven-descriptors -->
         <xsl:for-each select="//weblogic-enterprise-bean/message-driven-descriptor">
            <xsl:call-template name="message-driven-container-configuration">
               <xsl:with-param name="wl-ejb-descriptor" select="."/>
               <xsl:with-param name="default-configuration" select="$StandardMessageDrivenBean"/>
            </xsl:call-template>
         </xsl:for-each>

      </xsl:element> <!-- container-configurations -->
   </xsl:template> <!-- conatiner-configurations -->


   <!-- ========================================================= -->
   <!-- template to generate container-configuration for entity   -->
   <!-- ========================================================= -->
   <xsl:template name="entity-container-configuration">
      <!-- template parameters -->
      <xsl:param name="wl-ejb-descriptor"/>
      <xsl:param name="default-configuration"/>

      <!-- save current weblogic-enterprise-bean -->
      <xsl:variable name="weblogic-enterprise-bean" select="$wl-ejb-descriptor/.."/>

      <!-- construct container-name for current bean -->
      <xsl:variable name="container-name" select="concat($weblogic-enterprise-bean/ejb-name, ' Container Configuration')"/>

      <!-- set current context to container-configuration in standardwl.xml -->
      <xsl:for-each select="document($standardwl)/jboss/container-configurations/container-configuration[container-name=$default-configuration]">

         <!-- start container-configuration -->
         <xsl:element name="container-configuration">

            <!-- container-name -->
            <xsl:element name="container-name">
               <xsl:value-of select="$container-name"/>
            </xsl:element>

            <!-- call-logging -->
            <xsl:copy-of select="./call-logging"/>

            <!-- container-invoker -->
            <xsl:copy-of select="./container-invoker"/>

            <!-- container-interceptors -->
            <xsl:copy-of select="./container-interceptors"/>

            <!-- client-interceptors -->
            <xsl:copy-of select="./client-interceptors"/>

            <!-- instance-pool -->
            <xsl:copy-of select="./instance-pool"/>

            <!-- instance-cache -->
            <xsl:copy-of select="./instance-cache"/>

            <!-- persistence-manager -->
            <xsl:copy-of select="./persistence-manager"/>

            <!-- transaction-manager -->
            <xsl:copy-of select="./transaction-manager"/>

            <!-- web-class-loader -->
            <xsl:copy-of select="./web-class-loader"/>

            <!-- locking-policy -->
            <xsl:copy-of select="./locking-policy"/>

            <!-- container-invoker-conf -->
            <xsl:element name="container-invoker-conf">
               <!-- set optimized value -->
               <xsl:element name="Optimized">
                  <xsl:choose>
                     <xsl:when test="$weblogic-enterprise-bean[enable-call-by-reference='false']">
                        <xsl:text>False</xsl:text>
                     </xsl:when>
                     <xsl:otherwise>
                        <xsl:text>True</xsl:text>
                     </xsl:otherwise>
                  </xsl:choose>
               </xsl:element> <!-- Optimized -->

               <!-- copy RMIObjectPort -->
               <xsl:copy-of select="./container-invoker-conf/RMIObjectPort"/>
            </xsl:element> <!-- container-invoker-conf -->

            <!-- container-cache-conf -->
            <xsl:element name="container-cache-conf">
               <!-- AFAIK, WL uses only LRU policy -->
               <xsl:copy-of select="./container-cache-conf/cache-policy"/>
 
               <xsl:element name="cache-policy-conf">
                  <!-- AFAIK, WL doesn't have an equivalent. So let's copy it from standard configuration -->
                  <!-- min-capacity -->
                  <xsl:variable name="min-capacity"
                                select="./container-cache-conf/cache-policy-conf/min-capacity"/>
                  <xsl:copy-of select="$min-capacity"/>
 
                  <!-- max-capacity -->
                  <xsl:element name="max-capacity">
                     <xsl:choose>
                        <!-- when max-beans-in-cache exists and greater then min-capacity -->
                        <xsl:when test="number($wl-ejb-descriptor/entity-cache/max-beans-in-cache) > number($min-capacity)">
                           <xsl:value-of select="$wl-ejb-descriptor/entity-cache/max-beans-in-cache"/>
                        </xsl:when>
                        <xsl:when test="number(./container-cache-conf/cache-policy-conf/max-capacity) > number($min-capacity)">
                           <xsl:value-of select="./container-cache-conf/cache-policy-conf/max-capacity"/>
                        </xsl:when>
                        <xsl:otherwise>
                           <xsl:value-of select="$min-capacity"/>
                        </xsl:otherwise>
                     </xsl:choose>
                  </xsl:element> <!-- max-capacity -->

                  <!-- AFAIK, WL doesn't have an equivalent. So let's copy it from standard configuration -->
                  <!-- overager-period -->
                  <xsl:copy-of select="./container-cache-conf/cache-policy-conf/overager-period"/>

                  <!-- max-bean-age -->
                  <xsl:choose>
                     <!-- when idle-timeout-seconds exists -->
                     <xsl:when test="$wl-ejb-descriptor/entity-cache[idle-timeout-seconds]">
                        <xsl:element name="max-bean-age">
                           <xsl:value-of select="$wl-ejb-descriptor/entity-cache/idle-timeout-seconds"/>
                        </xsl:element> <!-- max-bean-age -->
                     </xsl:when>
                     <xsl:otherwise>
                        <xsl:copy-of select="./container-cache-conf/cache-policy-conf/max-bean-age"/>
                     </xsl:otherwise>
                  </xsl:choose>

                  <!-- AFAIK, WL doesn't have an equivalent. So let's copy it from standard configuration -->
                  <xsl:copy-of select="./container-cache-conf/cache-policy-conf/resizer-period"/>
                  <xsl:copy-of select="./container-cache-conf/cache-policy-conf/max-cache-miss-period"/>
                  <xsl:copy-of select="./container-cache-conf/cache-policy-conf/min-cache-miss-period"/>
                  <xsl:copy-of select="./container-cache-conf/cache-policy-conf/cache-load-factor"/>
               </xsl:element> <!-- cache-policy-conf -->
            </xsl:element> <!-- container-cache-conf -->

            <!-- container-pool-conf -->
            <xsl:element name="container-pool-conf">
               <!-- MaximumSize -->
               <xsl:choose>
                  <!-- when max-beans-in-free-pool exists -->
                  <xsl:when test="$wl-ejb-descriptor/pool[max-beans-in-free-pool]">
                     <xsl:element name="MaximumSize">
                        <xsl:value-of select="$wl-ejb-descriptor/pool/max-beans-in-free-pool"/>
                     </xsl:element> <!-- MaximumSize -->
                  </xsl:when>
                  <xsl:otherwise>
                     <xsl:copy-of select="./container-pool-conf/MaximumSize"/>
                  </xsl:otherwise>
               </xsl:choose>

               <xsl:copy-of select="./container-pool-conf/feeder-policy"/>
               <xsl:copy-of select="./container-pool-conf/feeder-policy-conf"/>
            </xsl:element> <!-- container-pool-conf -->

            <!-- commit-option -->
            <xsl:choose>
               <xsl:when test="$wl-ejb-descriptor/entity-cache/concurrency-strategy[text()='ReadOnly']">
                  <xsl:choose>
                     <xsl:when test="$wl-ejb-descriptor/entity-cache[read-timeout-seconds=0]">
                        <xsl:element name="commit-option">A</xsl:element>
                     </xsl:when> <!-- read-timeout-seconds = 0  -->
                     <xsl:otherwise>
                        <xsl:element name="commit-option">D</xsl:element>
                        <xsl:element name="optiond-refresh-rate">
                           <xsl:value-of select="$wl-ejb-descriptor/entity-cache/read-timeout-seconds"/>
                        </xsl:element> <!-- optiond-refresh-rate -->
                     </xsl:otherwise>
                  </xsl:choose>                           
               </xsl:when> <!-- concurrency-strategy = ReadOnly -->

               <!-- for versions 6.x -->
               <xsl:when test="$wl-ejb-descriptor/persistence[db-is-shared='false']">
                  <xsl:element name="commit-option">A</xsl:element>
               </xsl:when> <!-- db-is-shared = false -->

               <!-- for versions 7.x -->
               <xsl:when test="$wl-ejb-descriptor/persistence[cache-between-transactions='true']">
                  <xsl:element name="commit-option">A</xsl:element>
               </xsl:when> <!-- cache-between-transactions = true -->

               <xsl:otherwise>
                  <xsl:element name="commit-option">C</xsl:element>
               </xsl:otherwise>
            </xsl:choose>

         </xsl:element> <!-- container-configuration -->
      </xsl:for-each> <!-- standardwl.xml:container-configuration -->
   </xsl:template> <!-- entity-container-configurations -->


   <!-- ========================================================= -->
   <!-- template to generate container-configuration for SLSBs    -->
   <!-- ========================================================= -->
   <xsl:template name="stateless-session-container-configuration">
      <!-- template parameters -->
      <xsl:param name="wl-ejb-descriptor"/>
      <xsl:param name="default-configuration"/>

      <!-- save current weblogic-enterprise-bean -->
      <xsl:variable name="weblogic-enterprise-bean" select="$wl-ejb-descriptor/.."/>

      <!-- construct container-name for current bean -->
      <xsl:variable name="container-name" select="concat($weblogic-enterprise-bean/ejb-name, ' Container Configuration')"/>

      <!-- set current context to container-configuration in standardwl.xml -->
      <xsl:for-each select="document($standardwl)/jboss/container-configurations/container-configuration[container-name=$default-configuration]">

         <!-- start container-configuration -->
         <xsl:element name="container-configuration">

            <!-- container-name -->
            <xsl:element name="container-name">
               <xsl:value-of select="$container-name"/>
            </xsl:element>

            <!-- call-logging -->
            <xsl:copy-of select="./call-logging"/>
      
            <!-- container-invoker -->
            <xsl:copy-of select="./container-invoker"/>

            <!-- container-interceptors -->
            <xsl:copy-of select="./container-interceptors"/>

            <!-- client-interceptors -->
            <xsl:copy-of select="./client-interceptors"/>

            <!-- instance-pool -->
            <xsl:copy-of select="./instance-pool"/>

            <!-- transaction-manager -->
            <xsl:copy-of select="./transaction-manager"/>

            <!-- web-class-loader -->
            <xsl:copy-of select="./web-class-loader"/>

            <!-- container-invoker-conf -->
            <xsl:element name="container-invoker-conf">
               <!-- set optimized value -->
               <xsl:element name="Optimized">
                  <xsl:choose>
                     <xsl:when test="$weblogic-enterprise-bean[enable-call-by-reference='false']">
                        <xsl:text>False</xsl:text>
                     </xsl:when>
                     <xsl:otherwise>
                        <xsl:text>True</xsl:text>
                     </xsl:otherwise>
                  </xsl:choose>
               </xsl:element> <!-- Optimized -->

               <!-- copy RMIObjectPort -->
               <xsl:copy-of select="./container-invoker-conf/RMIObjectPort"/>
            </xsl:element> <!-- container-invoker-conf -->

            <!-- container-pool-conf -->
            <xsl:element name="container-pool-conf">
               <!-- MaximumSize -->
               <xsl:choose>
               <!-- when max-beans-in-free-pool exists -->
               <xsl:when test="$wl-ejb-descriptor/pool[max-beans-in-free-pool]">
                  <xsl:element name="MaximumSize">
                     <xsl:value-of select="$wl-ejb-descriptor/pool/max-beans-in-free-pool"/>
                  </xsl:element> <!-- MaximumSize -->
               </xsl:when>
               <xsl:otherwise>
                  <xsl:copy-of select="./container-pool-conf/MaximumSize"/>
               </xsl:otherwise>
            </xsl:choose>

            <xsl:copy-of select="./container-pool-conf/feeder-policy"/>
               <xsl:copy-of select="./container-pool-conf/feeder-policy-conf"/>
            </xsl:element> <!-- container-pool-conf -->

         </xsl:element> <!-- container-configuration -->
      </xsl:for-each> <!-- standardwl.xml:container-configuration -->
   </xsl:template> <!-- stateless-session-container-configuration -->


   <!-- ========================================================= -->
   <!-- template to generate SFSB container-configuration         -->
   <!-- ========================================================= -->
   <xsl:template name="stateful-session-container-configuration">
      <!-- template parameters -->
      <xsl:param name="wl-ejb-descriptor"/>
      <xsl:param name="default-configuration"/>

      <!-- save current weblogic-enterprise-bean -->
      <xsl:variable name="weblogic-enterprise-bean" select="$wl-ejb-descriptor/.."/>

      <!-- construct container-name for current bean -->
      <xsl:variable name="container-name" select="concat($weblogic-enterprise-bean/ejb-name, ' Container Configuration')"/>

      <!-- set current context to container-configuration in standardwl.xml -->
      <xsl:for-each select="document($standardwl)/jboss/container-configurations/container-configuration[container-name=$default-configuration]">

         <!-- start container-configuration -->
         <xsl:element name="container-configuration">

            <!-- container-name -->
            <xsl:element name="container-name">
               <xsl:value-of select="$container-name"/>
            </xsl:element>

            <!-- call-logging -->
            <xsl:copy-of select="./call-logging"/>

            <!-- container-invoker -->
            <xsl:copy-of select="./container-invoker"/>

            <!-- container-interceptors -->
            <xsl:copy-of select="./container-interceptors"/>

            <!-- client-interceptors -->
            <xsl:copy-of select="./client-interceptors"/>

            <!-- instance-pool -->
            <xsl:copy-of select="./instance-pool"/>

            <!-- instance-cache -->
            <xsl:copy-of select="./instance-cache"/>

            <!-- persistence-manager -->
            <xsl:copy-of select="./persistence-manager"/>

            <!-- transaction-manager -->
            <xsl:copy-of select="./transaction-manager"/>

            <!-- web-class-loader -->
            <xsl:copy-of select="./web-class-loader"/>

            <!-- container-invoker-conf -->
            <xsl:element name="container-invoker-conf">
               <!-- set optimized value -->
               <xsl:element name="Optimized">
                  <xsl:choose>
                     <xsl:when test="$weblogic-enterprise-bean[enable-call-by-reference='false']">
                        <xsl:text>False</xsl:text>
                     </xsl:when>
                     <xsl:otherwise>
                        <xsl:text>True</xsl:text>
                     </xsl:otherwise>
                  </xsl:choose>
               </xsl:element> <!-- Optimized -->

               <!-- copy RMIObjectPort -->
               <xsl:copy-of select="./container-invoker-conf/RMIObjectPort"/>
            </xsl:element> <!-- container-invoker-conf -->

            <!-- container-pool-conf -->
            <xsl:element name="container-pool-conf">
               <!-- MaximumSize -->
               <xsl:choose>
                  <!-- when max-beans-in-free-pool exists -->
                  <xsl:when test="$wl-ejb-descriptor/pool[max-beans-in-free-pool]">
                     <xsl:element name="MaximumSize">
                        <xsl:value-of select="$wl-ejb-descriptor/pool/max-beans-in-free-pool"/>
                     </xsl:element> <!-- MaximumSize -->
                  </xsl:when>
                  <xsl:otherwise>
                     <xsl:copy-of select="./container-pool-conf/MaximumSize"/>
                  </xsl:otherwise>
               </xsl:choose>

               <xsl:copy-of select="./container-pool-conf/feeder-policy"/>
               <xsl:copy-of select="./container-pool-conf/feeder-policy-conf"/>
            </xsl:element> <!-- container-pool-conf -->

            <!-- container-cache-conf -->
            <xsl:element name="container-cache-conf">
               <xsl:copy-of select="./container-cache-conf/cache-policy"/>

               <!-- cache-policy-conf -->
               <xsl:element name="cache-policy-conf">
                  <!-- min-capacity -->
                  <xsl:variable name="min-capacity"
                                select="./container-cache-conf/cache-policy-conf/min-capacity"/>
                  <xsl:copy-of select="$min-capacity"/>

                  <!-- max-capacity -->
                  <xsl:element name="max-capacity">
                     <xsl:choose>
                        <!-- when max-beans-in-cache exists and greater then min-capacity -->
                        <xsl:when test="number($wl-ejb-descriptor/stateful-session-cache/max-beans-in-cache) > number($min-capacity)">
                           <xsl:value-of select="$wl-ejb-descriptor/stateful-session-cache/max-beans-in-cache"/>
                        </xsl:when>
                        <xsl:when test="number(./container-cache-conf/cache-policy-conf/max-capacity) > number($min-capacity)">
                           <xsl:value-of select="./container-cache-conf/cache-policy-conf/max-capacity"/>
                        </xsl:when>
                        <xsl:otherwise>
                           <xsl:value-of select="$min-capacity"/>
                        </xsl:otherwise>
                     </xsl:choose>
                  </xsl:element> <!-- max-capacity -->

                  <!-- remover-period -->
                  <xsl:copy-of select="./container-cache-conf/cache-policy-conf/remover-period"/>

                  <!-- max-bean-life -->
                  <xsl:copy-of select="./container-cache-conf/cache-policy-conf/max-bean-life"/>

                  <!-- overager-period -->
                  <xsl:copy-of select="./container-cache-conf/cache-policy-conf/overager-period"/>

                  <!-- max-bean-age -->
                  <xsl:choose>
                     <xsl:when test="$wl-ejb-descriptor/stateful-session-cache[idle-timeout-seconds]">
                        <xsl:element name="max-bean-age">
                           <xsl:value-of select="$wl-ejb-descriptor/stateful-session-cache/idle-timeout-seconds"/>
                        </xsl:element> <!-- max-bean-age -->
                     </xsl:when> <!-- idle-timeout-seconds -->
                     <xsl:otherwise>
                        <xsl:copy-of select="./container-cache-conf/cache-policy-conf/max-bean-age"/>
                     </xsl:otherwise>
                  </xsl:choose>

                  <xsl:copy-of select="./container-cache-conf/cache-policy-conf/resizer-period"/>
                  <xsl:copy-of select="./container-cache-conf/cache-policy-conf/max-cache-miss-period"/>
                  <xsl:copy-of select="./container-cache-conf/cache-policy-conf/min-cache-miss-period"/>
                  <xsl:copy-of select="./container-cache-conf/cache-policy-conf/cache-load-factor"/>
               </xsl:element> <!-- cache-policy-conf -->
            </xsl:element> <!-- container-cache-conf -->

         </xsl:element> <!-- container-configuration -->

      </xsl:for-each>
   </xsl:template> <!-- stateful-session-container-configuration -->


   <!-- ========================================================= -->
   <!-- template to generate container-configuration for MDBs     -->
   <!-- ========================================================= -->
   <xsl:template name="message-driven-container-configuration">
      <!-- template parameters -->
      <xsl:param name="wl-ejb-descriptor"/>
      <xsl:param name="default-configuration"/>

      <!-- save current weblogic-enterprise-bean -->
      <xsl:variable name="weblogic-enterprise-bean" select="$wl-ejb-descriptor/.."/>

      <!-- construct container-name for current bean -->
      <xsl:variable name="container-name" select="concat($weblogic-enterprise-bean/ejb-name, ' Container Configuration')"/>

      <!-- set current context to container-configuration in standardwl.xml -->
      <xsl:for-each select="document($standardwl)/jboss/container-configurations/container-configuration[container-name=$default-configuration]">

         <!-- start container-configuration -->
         <xsl:element name="container-configuration">

            <!-- container-name -->
            <xsl:element name="container-name">
               <xsl:value-of select="$container-name"/>
            </xsl:element>

            <!-- call-logging -->
            <xsl:copy-of select="./call-logging"/>

            <!-- container-invoker -->
            <xsl:copy-of select="./container-invoker"/>

            <!-- container-interceptors -->
            <xsl:copy-of select="./container-interceptors"/>

            <!-- client-interceptors -->
            <xsl:copy-of select="./client-interceptors"/>

            <!-- instance-pool -->
            <xsl:copy-of select="./instance-pool"/>

            <!-- transaction-manager -->
            <xsl:copy-of select="./transaction-manager"/>

            <!-- web-class-loader -->
            <xsl:copy-of select="./web-class-loader"/>

            <!-- container-invoker-conf -->
            <xsl:element name="container-invoker-conf">
               <!-- set optimized value -->
               <xsl:element name="Optimized">
                  <xsl:choose>
                     <xsl:when test="$weblogic-enterprise-bean[enable-call-by-reference='false']">
                        <xsl:text>False</xsl:text>
                     </xsl:when>
                     <xsl:otherwise>
                        <xsl:text>True</xsl:text>
                     </xsl:otherwise>
                  </xsl:choose>
               </xsl:element> <!-- Optimized -->

               <!-- copy RMIObjectPort -->
               <xsl:copy-of select="./container-invoker-conf/RMIObjectPort"/>
            </xsl:element> <!-- container-invoker-conf -->

            <!-- container-pool-conf -->
            <xsl:element name="container-pool-conf">
               <!-- MaximumSize -->
               <xsl:choose>
                  <!-- when max-beans-in-free-pool exists -->
                  <xsl:when test="$wl-ejb-descriptor/pool[max-beans-in-free-pool]">
                     <xsl:element name="MaximumSize">
                        <xsl:value-of select="$wl-ejb-descriptor/pool/max-beans-in-free-pool"/>
                     </xsl:element> <!-- MaximumSize -->
                  </xsl:when>
                  <xsl:otherwise>
                     <xsl:copy-of select="./container-pool-conf/MaximumSize"/>
                  </xsl:otherwise>
               </xsl:choose>

               <xsl:copy-of select="./container-pool-conf/feeder-policy"/>
               <xsl:copy-of select="./container-pool-conf/feeder-policy-conf"/>
            </xsl:element> <!-- container-pool-conf -->

         </xsl:element> <!-- container-configuration -->
      </xsl:for-each> <!-- standardwl.xml:container-configuration -->
   </xsl:template> <!-- message-driven-container-configuration -->

</xsl:stylesheet>
