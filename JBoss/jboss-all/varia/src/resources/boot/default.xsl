<?xml version="1.0"?> 
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:java="http://xml.apache.org/xslt/java"
                version="1.0">

  <!-- Define variables that are to be replaced -->
  <xsl:variable name="INSTALL_PATH" select="'/tmp/jboss'"/>
  <xsl:variable name="CONF_NAME" select="'default'"/>

  <!-- The transformation definitions -->
  <xsl:template match='/'>
    <xsl:apply-templates />
  </xsl:template>

  <xsl:template match='mbean/attribute/text()'>
    <xsl:variable name="text" select="."/>
    <!-- Replace any variable reference -->
    <xsl:value-of select="java:org.jboss.boot.servlets.Util.replaceVariables($text)"/>
  </xsl:template>

  <!-- Copy all other elements -->
  <xsl:template match='*|@*|comment()'>
    <xsl:copy>
      <xsl:apply-templates select='@*|node()'/>
    </xsl:copy>
  </xsl:template>

</xsl:stylesheet>
