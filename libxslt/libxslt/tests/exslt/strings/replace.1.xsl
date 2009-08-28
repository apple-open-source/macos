<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:str="http://exslt.org/strings"
    exclude-result-prefixes="str">

<xsl:template match="/">
	<xsl:variable name="x" select="doc/strings/x"/>
	<xsl:variable name="y" select="doc/strings/y"/>

<out>;
	str:replace('a, simple, list', ', ', '-')
	<xsl:copy-of select="str:replace('a, simple, list', ', ', '-')"/>

	str:replace('a, simple, list', 'a, ', 'the ')
	<xsl:copy-of select="str:replace('a, simple, list', 'a, ', 'the ')"/>

	str:replace('a, simple, list', 'list', 'array')
	<xsl:copy-of select="str:replace('a, simple, list', 'list', 'array')"/>

	str:replace('a, simple, list', 'i', 'I')
	<xsl:copy-of select="str:replace('a, simple, list', 'i', 'I')"/>

	str:replace('a, simple, list', ', ', '')
	<xsl:copy-of select="str:replace('a, simple, list', ', ', '')"/>

	str:replace('fee, fi, fo, fum', $x, $y)
	<xsl:copy-of select="str:replace('fee, fi, fo, fum', $x, $y)" />

	str:replace('fee, fi, fo, fum', $x, 'j')
	<xsl:copy-of select="str:replace('fee, fi, fo, fum', $x, 'j')" />

</out>
</xsl:template>

</xsl:stylesheet>
