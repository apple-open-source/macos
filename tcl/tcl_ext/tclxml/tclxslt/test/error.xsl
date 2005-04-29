<xsl:stylesheet version='1.0' xmlns:xsl='http://www.w3.org/1999/XSL/Transform'>

  <xsl:template match="/">
    <xsl:text>This should be an error</xsl:text>
    <xsl:apply-templates match="whatever"/>
  </xsl:template>

  <xsl:template/>

</xsl:stylesheet>

