<?xml version="1.0" encoding="UTF-8"?>

<!--
			X.Org DocBook/XML customization

	DocBook XSL Stylesheets HTML Parameters
	http://docbook.sourceforge.net/release/xsl/current/doc/html/
-->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version='1.0'>
<xsl:import href="http://docbook.sourceforge.net/release/xsl/current/xhtml/docbook.xsl"/>

			<!-- Reference Pages HTML/FO Parameters -->

  <!-- The formatting of a function element will include generated parentheses -->
  <xsl:param name="function.parens" select="1"/>

  <!-- ANSI-style function synopses are generated for a funcsynopsis element -->
  <xsl:param name="funcsynopsis.style" select="ansi"/>

			<!-- Miscellaneous HTML/FO Parameters-->

  <!-- SVG will be considered an acceptable image format -->
  <xsl:param name="use.svg" select="1"/>

			<!-- Pagination and General Styles HTML/FO Parameters -->
  <!--
     Speed up ps & pdf creation by not creating pages with "draft" image,
     thus not needing to wait for http fetch of draft.png from docbook website.
    -->
  <xsl:param name="draft.mode" select="no"/>

			<!-- ToC/LoT/Index Generation HTML Parameters -->

  <!-- Index links should point to indexterm location, not start of section -->
  <xsl:param name="index.links.to.section" select="0"/>

			<!-- HTML Parameters -->

  <!-- Uses XSLT Extension to provide more valid and better formatted elements-->
  <xsl:param name="html.cleanup" select="1"/>

			<!-- Meta/*Info and Titlepages HTML Parameters-->

  <!-- Suppress abstract on title pages -->
  <xsl:param name="abstract.notitle.enabled" select="1"/>

  <!-- Lists HTML Parameters-->
  <xsl:param name="variablelist.as.table" select="1"/>

</xsl:stylesheet>
