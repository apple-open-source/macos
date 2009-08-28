<xsl:stylesheet version='1.0' xmlns:xsl='http://www.w3.org/1999/XSL/Transform'>

  <xsl:import href='../testB/test.xsl'/>

  <xsl:template match="para">
    <xsl:text>Test A para template</xsl:text>
    <xsl:apply-templates/>
  </xsl:template>

</xsl:stylesheet>

