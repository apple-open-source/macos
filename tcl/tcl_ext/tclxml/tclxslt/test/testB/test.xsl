<xsl:stylesheet version='1.0' xmlns:xsl='http://www.w3.org/1999/XSL/Transform'>

  <xsl:template match="para">
    <xsl:text>Test B para template</xsl:text>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="Test">
    <xsl:text>Test B Test template</xsl:text>
    <xsl:apply-templates/>
  </xsl:template>

</xsl:stylesheet>

