<?xml version="1.0" encoding="UTF-8"?>

<!--
			X.Org DocBook/XML customization

	DocBook XSL Stylesheets FO Parameters
	http://docbook.sourceforge.net/release/xsl/current/doc/fo/
-->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version='1.0'>
<xsl:import href="http://docbook.sourceforge.net/release/xsl/current/fo/docbook.xsl"/>


			<!-- Reference Pages HTML/FO Parameters -->

  <!-- The formatting of a function element will include generated parentheses -->
  <xsl:param name="function.parens" select="1"/>

  <!-- ANSI-style function synopses are generated for a funcsynopsis element -->
  <xsl:param name="funcsynopsis.style" select="ansi"/>

			<!-- Linking HTML/FO Parameters -->

  <!-- open new PDF documents in new tab, don't replace doc in current window -->
  <xsl:attribute-set name="olink.properties">
    <xsl:attribute name="show-destination">new</xsl:attribute>
  </xsl:attribute-set>

			<!-- Miscellaneous HTML/FO Parameters-->

  <!-- SVG will be considered an acceptable image format -->
  <xsl:param name="use.svg" select="1"/>

			<!-- Pagination and General Styles FO Parameters -->
  <!--
     Speed up ps & pdf creation by not creating pages with "draft" image,
     thus not needing to wait for http fetch of draft.png from docbook website.
    -->
  <xsl:param name="draft.mode" select="no"/>

			<!-- Processor Extensions FO Parameters-->

  <!-- PDF bookmarks extensions for FOP version 0.90 and later will be used. -->
  <xsl:param name="fop.extensions" select="0"></xsl:param>
  <xsl:param name="fop1.extensions" select="1"></xsl:param>

			<!-- Cross Refrences FO Parameters-->

  <!-- Make links in pdf output blue so it's easier to tell they're internal
       links
   -->
  <xsl:attribute-set name="xref.properties">
    <xsl:attribute name="color">blue</xsl:attribute>
  </xsl:attribute-set>

  <!-- Make links in pdf output green so it's easier to tell they're external
       links
  -->
  <xsl:attribute-set name="olink.properties">
    <xsl:attribute name="color">green</xsl:attribute>
  </xsl:attribute-set>

  <!-- Linking to a target inside a pdf document.
       This feature is only available as of docbook-xsl-1.76.1.
       When set to zero, the link will point to the document -->
  <xsl:param name="insert.olink.pdf.frag" select="0"></xsl:param>


			<!-- Font Families FO Parameters -->

  <!--
     Since a number of documents, especially the credits section in the
     ReleaseNotes, use characters not found in the fop default base-14
     PostScript fonts, set the fonts for the fop generated documents to
     use the free DejaVu and GNU Unifont fonts which cover a much wider
     range of characters.

     DejaVu is available from http://dejavu-fonts.org/
     GNU Unifont is available from http://unifoundry.com/unifont.html

     To set fop font paths to find them after installing, see
     http://xmlgraphics.apache.org/fop/1.0/fonts.html#basics
    -->
  <xsl:param name="body.font.family">DejaVu Serif</xsl:param>
  <xsl:param name="symbol.font.family">serif,Symbol,AR PL UMing CN,AR PL ShanHeiSun Uni,GNU Unifont</xsl:param>

</xsl:stylesheet>
