<?xml version="1.0" encoding="utf-8"?>

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

  <xsl:output method="xml" indent="yes"/>

  <!--top level template converts to top level "server" tag-->
  <xsl:template match="datasources|connection-factories"><!--|server|service"-->

    <server>

      <xsl:apply-templates/>

    </server>

  </xsl:template>


  <!-- template for generic resource adapters supporting transactions -->
  <xsl:template match="tx-connection-factory">

    <mbean code="org.jboss.resource.connectionmanager.TxConnectionManager" name="jboss.jca:service=TxCM,name={jndi-name}" display-name="ConnectionManager for ConnectionFactory {jndi-name}">

      <xsl:choose>
        <xsl:when test="(xa-transaction) and (track-connection-by-tx)">
          <attribute name="TrackConnectionByTx">true</attribute>
          <attribute name="LocalTransactions">false</attribute>
        </xsl:when>
        <xsl:when test="(xa-transaction)">
          <attribute name="TrackConnectionByTx">false</attribute>
          <attribute name="LocalTransactions">false</attribute>
        </xsl:when>
        <xsl:otherwise>
          <attribute name="TrackConnectionByTx">true</attribute>
          <attribute name="LocalTransactions">true</attribute>
        </xsl:otherwise>
      </xsl:choose>

      <xsl:call-template name="pool">
        <xsl:with-param name="mcf-template">generic-mcf</xsl:with-param>
      </xsl:call-template>
      <xsl:call-template name="cm-common"/>
      <xsl:call-template name="tx-manager"/>

    </mbean>
  </xsl:template>


  <!--template for generic resource adapters that do not support transactions-->
  <xsl:template match="no-tx-connection-factory">

    <mbean code="org.jboss.resource.connectionmanager.NoTxConnectionManager" name="jboss.jca:service=NoTxCM,name={jndi-name}" display-name="ConnectionManager for ConnectionFactory {jndi-name}">

      <xsl:call-template name="pool">
        <xsl:with-param name="mcf-template">generic-mcf</xsl:with-param>
      </xsl:call-template>
      <xsl:call-template name="cm-common"/>
    </mbean>
  </xsl:template>


  <!-- Template for our jca-jdbc non-XADatasource (local) wrapper, using local transactions. -->
  <xsl:template match="local-tx-datasource">

    <mbean code="org.jboss.resource.connectionmanager.TxConnectionManager" name="jboss.jca:service=LocalTxCM,name={jndi-name}" display-name="ConnectionManager for DataSource {jndi-name}">

      <attribute name="TrackConnectionByTx">true</attribute>
      <attribute name="LocalTransactions">true</attribute>

      <xsl:call-template name="pool">
        <xsl:with-param name="mcf-template">local-wrapper</xsl:with-param>
      </xsl:call-template>
      <xsl:call-template name="cm-common"/>
      <xsl:call-template name="tx-manager"/>

   </mbean>
  </xsl:template>

  <!-- Template for our jca-jdbc non-XADatasource (local) wrapper, using no transactions. -->
  <xsl:template match="no-tx-datasource">

    <mbean code="org.jboss.resource.connectionmanager.NoTxConnectionManager" name="jboss.jca:service=NoTxCM,name={jndi-name}">

      <xsl:call-template name="pool">
        <xsl:with-param name="mcf-template">local-wrapper</xsl:with-param>
      </xsl:call-template>
      <xsl:call-template name="cm-common"/>
    </mbean>
  </xsl:template>

  <!-- Template for our jca-jdbc XADatasource wrapper. -->
  <xsl:template match="xa-datasource">

    <mbean code="org.jboss.resource.connectionmanager.TxConnectionManager" 
          name="jboss.jca:service=XATxCM,name={jndi-name}">

      <xsl:choose>
        <xsl:when test="track-connection-by-tx">
          <attribute name="TrackConnectionByTx">true</attribute>
          <attribute name="LocalTransactions">false</attribute>
        </xsl:when>
        <xsl:otherwise>
          <attribute name="TrackConnectionByTx">false</attribute>
          <attribute name="LocalTransactions">false</attribute>
        </xsl:otherwise>
      </xsl:choose>

      <xsl:call-template name="pool">
        <xsl:with-param name="mcf-template">xa-wrapper</xsl:with-param>
      </xsl:call-template>
      <xsl:call-template name="cm-common"/>
      <xsl:call-template name="tx-manager"/>

    </mbean>
  </xsl:template>

  <!-- template to generate a property file format from a set of               -->
  <!-- <xa-datasource-property name="blah">blah-value</xa-datasource-property> -->
  <!-- or                                                                      -->
  <!-- <connection-property name="foo">bar</connection-property>               -->
  <!-- tags. The newline in the xsl:text element is crucial!                   -->
  <!-- this makes a property file format, not the ; delimited format-->
  <xsl:template match="xa-datasource-property|connection-property">
    <xsl:value-of select="@name"/>=<xsl:value-of select="normalize-space(.)"/><xsl:text>
</xsl:text>
  </xsl:template>

  <!-- template to generate the ManagedConnectionFactory mbean for a generic jca adapter -->
  <xsl:template name="generic-mcf">
      <depends optional-attribute-name="ManagedConnectionFactoryName">
      <!--embedded mbean-->
        <mbean code="org.jboss.resource.connectionmanager.RARDeployment" name="jboss.jca:service=ManagedConnectionFactory,name={jndi-name}" display-name="ManagedConnectionFactory for ConnectionFactory {jndi-name}">

          <xsl:apply-templates select="depends" mode="anonymous"/>
          <attribute name="ManagedConnectionFactoryProperties">
            <properties>

              <!--we need the other standard properties here-->
              <xsl:if test="user-name">
                <config-property name="UserName" type="java.lang.String"><xsl:value-of select="normalize-space(user-name)"/></config-property>
              </xsl:if>
              <xsl:if test="password">
                <config-property name="Password" type="java.lang.String"><xsl:value-of select="normalize-space(password)"/></config-property>
              </xsl:if>
              <xsl:apply-templates select="config-property"/>
            </properties>
          </attribute>

          <depends optional-attribute-name="OldRarDeployment">jboss.jca:service=RARDeployment,name=<xsl:value-of select="adapter-display-name"/></depends>

        </mbean>
      </depends>
  </xsl:template>

  <!-- template to copy config-property elements.  This actually does a literal copy -->
  <!-- Please keep this for consistency with the jb4 version which does not do a literal copy -->
  <xsl:template match="config-property">
    <config-property name="{@name}" type="{@type}"><xsl:apply-templates/></config-property>
  </xsl:template>

  <!-- template to generate the ManagedConnectionFactory mbean for our jca-jdbc local wrapper -->
  <xsl:template name="local-wrapper">

      <depends optional-attribute-name="ManagedConnectionFactoryName">
      <!--embedded mbean-->
        <mbean code="org.jboss.resource.connectionmanager.RARDeployment" name="jboss.jca:service=ManagedConnectionFactory,name={jndi-name}" display-name="ManagedConnectionFactory for DataSource {jndi-name}">

          <xsl:apply-templates select="depends" mode="anonymous"/>

          <depends optional-attribute-name="OldRarDeployment">jboss.jca:service=RARDeployment,name=JBoss LocalTransaction JDBC Wrapper</depends>

          <attribute name="ManagedConnectionFactoryProperties">
            <properties>
              <config-property name="ConnectionURL" type="java.lang.String"><xsl:value-of select="normalize-space(connection-url)"/></config-property>
              <config-property name="DriverClass" type="java.lang.String"><xsl:value-of select="normalize-space(driver-class)"/></config-property>

              <xsl:call-template name="wrapper-common-properties"/>
              <xsl:if test="connection-property">
                <config-property name="ConnectionProperties" type="java.lang.String">
                  <xsl:apply-templates select="connection-property"/>
                </config-property>
              </xsl:if>

            </properties>
          </attribute>
        </mbean>
      </depends>
  </xsl:template>

  <xsl:template name="xa-wrapper">
     <depends optional-attribute-name="ManagedConnectionFactoryName">
        <!--embedded mbean-->
        <mbean code="org.jboss.resource.connectionmanager.RARDeployment"
              name="jboss.jca:service=ManagedConnectionFactory,name={jndi-name}">
              displayname="ManagedConnectionFactory for DataSource {jndi-name}">
          
          <xsl:apply-templates select="depends" mode="anonymous"/>

          <depends optional-attribute-name="OldRarDeployment">jboss.jca:service=RARDeployment,name=JBoss JDBC XATransaction ResourceAdapter</depends>

          <attribute name="ManagedConnectionFactoryProperties">
            <properties>
              <config-property name="XADataSourceClass" type="java.lang.String"><xsl:value-of select="normalize-space(xa-datasource-class)"/></config-property>

              <config-property name="XADataSourceProperties" type="java.lang.String">
                <xsl:apply-templates select="xa-datasource-property"/>
              </config-property>

              <xsl:if test="isSameRM-override-value">
                <config-property name="IsSameRMOverrideValue" type="java.lang.Boolean"><xsl:value-of select="normalize-space(isSameRM-override-value)"/></config-property>
              </xsl:if>

              <xsl:call-template name="wrapper-common-properties"/>

            </properties>
          </attribute>
        </mbean>
      </depends>
  </xsl:template>

  <!-- template for the ManagedConnectionFactory properties shared between our local and xa wrappers -->
  <xsl:template name="wrapper-common-properties">

          <xsl:if test="transaction-isolation">
            <config-property name="TransactionIsolation" type="java.lang.String"><xsl:value-of select="normalize-space(transaction-isolation)"/></config-property>
          </xsl:if>

          <xsl:if test="user-name">
            <config-property name="UserName" type="java.lang.String"><xsl:value-of select="normalize-space(user-name)"/></config-property>
          </xsl:if>
          <xsl:if test="password">
            <config-property name="Password" type="java.lang.String"><xsl:value-of select="normalize-space(password)"/></config-property>
          </xsl:if>
          <xsl:if test="new-connection-sql">
            <config-property name="NewConnectionSQL" type="java.lang.String"><xsl:value-of select="normalize-space(new-connection-sql)"/></config-property>
          </xsl:if>
          <xsl:if test="check-valid-connection-sql">
            <config-property name="CheckValidConnectionSQL" type="java.lang.String"><xsl:value-of select="normalize-space(check-valid-connection-sql)"/></config-property>
          </xsl:if>
          <xsl:if test="valid-connection-checker-class-name">
            <config-property name="ValidConnectionCheckerClassName" type="java.lang.String"><xsl:value-of select="normalize-space(valid-connection-checker-class-name)"/></config-property>
          </xsl:if>
          <xsl:if test="exception-sorter-class-name">
            <config-property name="ExceptionSorterClassName" type="java.lang.String"><xsl:value-of select="normalize-space(exception-sorter-class-name)"/></config-property>
          </xsl:if>
          <xsl:if test="track-statements">
            <config-property name="TrackStatements" type="boolean"><xsl:value-of select="normalize-space(track-statements)"/></config-property>
          </xsl:if>
          <xsl:if test="prepared-statement-cache-size">
            <config-property name="PreparedStatementCacheSize" type="int"><xsl:value-of select="normalize-space(prepared-statement-cache-size)"/></config-property>
          </xsl:if>
          <xsl:if test="set-tx-query-timeout">
            <config-property name="TxQueryTimeout" type="int"><xsl:value-of select="normalize-space(set-tx-query-timeout)"/></config-property>
          </xsl:if>
  </xsl:template>

  <!-- template to generate the pool mbean -->
  <xsl:template name="pool">
      <xsl:param name="mcf-template">generic-mcf</xsl:param>
      <depends optional-attribute-name="ManagedConnectionPool">

        <!--embedded mbean-->
        <mbean code="org.jboss.resource.connectionmanager.JBossManagedConnectionPool" name="jboss.jca:service=ManagedConnectionPool,name={jndi-name}" display-name="Connection Pool for DataSource {jndi-name}">
          <xsl:choose>
            <xsl:when test="$mcf-template='generic-mcf'">
              <xsl:call-template name="generic-mcf"/>
            </xsl:when>
            <xsl:when test="$mcf-template='local-wrapper'">
              <xsl:call-template name="local-wrapper"/>
            </xsl:when>
            <xsl:when test="$mcf-template='xa-wrapper'">
              <xsl:call-template name="xa-wrapper"/>
            </xsl:when>
          </xsl:choose>

          <xsl:choose>
            <xsl:when test="min-pool-size">
              <attribute name="MinSize"><xsl:value-of select="min-pool-size"/></attribute>
            </xsl:when>
            <xsl:otherwise>
              <attribute name="MinSize">0</attribute>
            </xsl:otherwise>
          </xsl:choose>
          <xsl:choose>
            <xsl:when test="max-pool-size">
              <attribute name="MaxSize"><xsl:value-of select="max-pool-size"/></attribute>
            </xsl:when>
            <xsl:otherwise>
              <attribute name="MaxSize">20</attribute>
            </xsl:otherwise>
          </xsl:choose>
          <xsl:choose>
            <xsl:when test="blocking-timeout-millis">
              <attribute name="BlockingTimeoutMillis"><xsl:value-of select="blocking-timeout-millis"/></attribute>
            </xsl:when>
            <xsl:otherwise>
              <attribute name="BlockingTimeoutMillis">5000</attribute>
            </xsl:otherwise>
          </xsl:choose>
          <xsl:choose>
            <xsl:when test="idle-timeout-minutes">
              <attribute name="IdleTimeoutMinutes"><xsl:value-of select="idle-timeout-minutes"/></attribute>
            </xsl:when>
            <xsl:otherwise>
              <attribute name="IdleTimeoutMinutes">15</attribute>
            </xsl:otherwise>
          </xsl:choose>
          <!--
		criteria indicates if Subject (from security domain) or app supplied
            parameters (such as from getConnection(user, pw)) are used to distinguish
            connections in the pool. Choices are 
            ByContainerAndApplication (use both), 
            ByContainer (use Subject),
            ByApplication (use app supplied params only),
            ByNothing (all connections are equivalent, usually if adapter supports
              reauthentication)-->
          <attribute name="Criteria">
	    <xsl:choose>
              <xsl:when test="application-managed-security">ByApplication</xsl:when>
              <xsl:when test="security-domain-and-application">ByContainerAndApplication</xsl:when>
              <xsl:when test="security-domain">ByContainer</xsl:when>
              <xsl:otherwise>ByNothing</xsl:otherwise>
            </xsl:choose>
          </attribute>
         <xsl:choose>
           <xsl:when test="no-tx-separate-pools">
             <attribute name="NoTxSeparatePools">true</attribute>
           </xsl:when>
         </xsl:choose>
        </mbean>
      </depends>
  </xsl:template>


  <!-- template for ConnectionManager attributes shared among all ConnectionManagers.-->
  <xsl:template name="cm-common">

      <attribute name="JndiName"><xsl:value-of select="jndi-name"/></attribute>
      <depends optional-attribute-name="CachedConnectionManager">jboss.jca:service=CachedConnectionManager</depends>

      <xsl:if test="security-domain|security-domain-and-application">
        <attribute name="SecurityDomainJndiName"><xsl:value-of select="security-domain|security-domain-and-application"/></attribute>
        <depends optional-attribute-name="JaasSecurityManagerService">jboss.security:service=JaasSecurityManager</depends>
      </xsl:if>

  </xsl:template>

  <xsl:template name="tx-manager">
      <depends optional-attribute-name="TransactionManagerService">jboss:service=TransactionManager</depends>
  </xsl:template>

  <!-- template to copy any anonymous depends elements inside a cf/ds configuration element -->
  <xsl:template match="depends" mode="anonymous">
    <depends><xsl:value-of select="."/></depends>
  </xsl:template>

  <!-- template to copy all other elements literally, mbeans for instance-->
  <xsl:template match="*|@*|text()">
    <xsl:copy>
      <xsl:apply-templates select="*|@*|text()"/>
    </xsl:copy>
  </xsl:template>

</xsl:stylesheet>
