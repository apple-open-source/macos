<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">
  <!--
Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
   -->

  <!--
    Shared stylesheet for X.Org documentation formatted in DocBook/XML
    -->
  <xsl:param name="html.cleanup" select="1"/>
  <xsl:param name="html.stylesheet" select="'xorg.css'"/>
  <xsl:param name="chunker.output.indent">yes</xsl:param>
  <xsl:param name="html.extra.head.links" select="1"/>
  <xsl:param name="saxon.character.representation" select="'entity;decimal'"/>
  <xsl:param name="function.parens" select="1"/>
  <xsl:param name="funcsynopsis.style" select="ansi"/>
  <xsl:param name="abstract.notitle.enabled" select="1"/>
  <xsl:param name="variablelist.as.table" select="1"/>
  <xsl:param name="use.svg" select="1"/>

  <!-- Index links should point to indexterm location, not start of section -->
  <xsl:param name="index.links.to.section" select="0"/>

  <!-- PDF bookmarks extensions for FOP version 0.90 and later will be used. -->
  <xsl:param name="fop.extensions" select="0"></xsl:param>
  <xsl:param name="fop1.extensions" select="1"></xsl:param>

  <!--
     Speed up ps & pdf creation by not creating pages with "draft" image,
     thus not needing to wait for http fetch of draft.png from docbook website.
    -->
  <xsl:param name="draft.mode" select="no"/>

  <!--
     Make links in pdf output blue so it's easier to tell they're links.
    -->
  <xsl:attribute-set name="xref.properties">
    <xsl:attribute name="color">blue</xsl:attribute>
  </xsl:attribute-set>

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
