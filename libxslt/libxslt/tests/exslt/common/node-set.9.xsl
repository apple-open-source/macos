<?xml version="1.0"?>

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"
xmlns:exslt="http://exslt.org/common"
extension-element-prefixes="exslt">

<!-- https://gitlab.gnome.org/GNOME/libxslt/-/issues/37 -->

<xsl:variable name="tree1">
  <a><b><c><d/></c></b></a>
</xsl:variable>
<xsl:variable name="tree2">
  <a><b><c><d/></c></b></a>
</xsl:variable>

<xsl:template match="a">
  <xsl:for-each select="/*">
    <match/>
  </xsl:for-each>
</xsl:template>

<xsl:template match="/">
  <out>
    <xsl:apply-templates select="exslt:node-set($tree1)/a | exslt:node-set($tree2)/a"/>
  </out>
</xsl:template>

</xsl:stylesheet>
