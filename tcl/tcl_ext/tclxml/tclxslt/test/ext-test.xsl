<xsl:stylesheet version='1.0'
	xmlns:xsl='http://www.w3.org/1999/XSL/Transform'
	xmlns:ext='http://tclxml.sf.net/XSLT/Test'
	extension-element-prefixes='ext'>

  <xsl:output method='text'/>

  <xsl:template match='/'>
    <xsl:text>Test value: "</xsl:text>
    <xsl:value-of select="ext:test('argument 1', 'argument 2')"/>
    <xsl:text>"</xsl:text>
  </xsl:template>

</xsl:stylesheet>
