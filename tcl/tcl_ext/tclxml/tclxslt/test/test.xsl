<xsl:stylesheet version='1.0' xmlns:xsl='http://www.w3.org/1999/XSL/Transform'>

  <xsl:import href='docbook/html/docbook.xsl'/>

  <xsl:template match="para">
    <xsl:text>Awesome!</xsl:text>
    <xsl:apply-imports/>
  </xsl:template>

</xsl:stylesheet>

