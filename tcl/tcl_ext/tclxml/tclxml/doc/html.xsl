<?xml version="1.0"?>

<!--
   - html.xsl -
   -
   - Copyright (c) 2000 Zveno Pty Ltd
   -
   -	XSLT stylesheet to convert DocBook+Tcl mods to HTML.
   -
   - $Id: html.xsl,v 1.2 2002/06/11 13:37:45 balls Exp $
   -->

<xsl:stylesheet version='1.0'
        xmlns:xsl='http://www.w3.org/1999/XSL/Transform'>

<!-- Import standard DocBook stylesheets
   -
   - We import, rather than include, so that templates defined here 
   - have higher priority.
   -
   - NB. 'docbook' must be a symbolic link (or equivalent) to
   - DocBook stylesheets (v1.14 or later).
  -->

<xsl:import href='docbook/xsl/html/docbook.xsl'/>

<!-- Provide a template which adds a TOC -->

<xsl:template match="refentry">
  <xsl:variable name="refmeta" select=".//refmeta"/>
  <xsl:variable name="refentrytitle" select="$refmeta//refentrytitle"/>
  <xsl:variable name="refnamediv" select=".//refnamediv"/>
  <xsl:variable name="refname" select="$refnamediv//refname"/>
  <xsl:variable name="title">
    <xsl:choose>
      <xsl:when test="$refentrytitle">
        <xsl:apply-templates select="$refentrytitle[1]" mode="title"/>
      </xsl:when>
      <xsl:when test="$refname">
        <xsl:apply-templates select="$refname[1]" mode="title"/>
      </xsl:when>
      <xsl:otherwise></xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <div class="{name(.)}">
    <h1 class="title">
      <a>
        <xsl:attribute name="name">
          <xsl:call-template name="object.id"/>
        </xsl:attribute>
        <xsl:copy-of select="$title"/>
      </a>
    </h1>
    <h2>Contents</h2>
    <ul>
      <xsl:if test="refsynopsisdiv">
	<li><a href="#synopsis">Synopsis</a></li>
      </xsl:if>
      <xsl:for-each select="refsect1">
	<xsl:variable name="sect1name" select="translate(title,' ','-')"/>
	<li>
	  <a href="#{$sect1name}"><xsl:value-of select="title"/></a>
	  <xsl:if test="refsect2">
	    <ul>
	      <xsl:for-each select="refsect2">
		<xsl:variable name="sect2name" select="translate(title,' ','-')"/>
		<li>
		  <a href="#{$sect1name}-{$sect2name}"><xsl:value-of select="title"/></a>
		  <xsl:if test="refsect3">
		    <ul>
		      <xsl:for-each select="refsect3">
			<xsl:variable name="sect3name" select="translate(title,' ','-')"/>
			<li>
			  <a href="#{$sect1name}-{$sect2name}-{$sect3name}"><xsl:value-of select="title"/></a>
			</li>
		      </xsl:for-each>
		    </ul>
		  </xsl:if>
		</li>
	      </xsl:for-each>
	    </ul>
	  </xsl:if>
	</li>
      </xsl:for-each>
    </ul>
    <xsl:apply-templates/>
    <xsl:call-template name="process.footnotes"/>
  </div>
</xsl:template>

<xsl:template match="refsynopsisdiv">
  <div class="{name(.)}">
    <a name="synopsis">
    </a>
    <h2>Synopsis</h2>
    <xsl:apply-templates select="*[name() != 'tclnamespacesynopsis']"/>
    <xsl:apply-templates select="tclnamespacesynopsis"/>
  </div>
</xsl:template>

<xsl:template match="tclcmdsynopsis">
  <xsl:variable name="id"><xsl:call-template name="object.id"/></xsl:variable>

  <div class="{name(.)}" id="{$id}">
    <a name="{$id}"/>
    <xsl:apply-templates/>
  </div>
</xsl:template>
<xsl:template match="tclcmdsynopsis/command">
  <br/>
  <xsl:call-template name="inline.monoseq"/>
  <xsl:text> </xsl:text>
</xsl:template>
<xsl:template match="tclcmdsynopsis/command[1]">
  <xsl:call-template name="inline.monoseq"/>
  <xsl:text> </xsl:text>
</xsl:template>

<xsl:template match="refsynopsisdiv/tclcmdsynopsis/command">
  <xsl:variable name="id"><xsl:call-template name="object.id"/></xsl:variable>

  <br/>
  <span class="{name(.)}" id="{$id}">
    <a name="{translate(.,': ','__')}"/>
    <xsl:call-template name="inline.monoseq"/>
    <xsl:text> </xsl:text>
  </span>
</xsl:template>
<xsl:template match="refsynopsisdiv/tclcmdsynopsis/command[1]">
  <xsl:variable name="id"><xsl:call-template name="object.id"/></xsl:variable>

  <span class="{name(.)}" id="{$id}">
    <a name="{translate(.,': ','__')}"/>
    <xsl:call-template name="inline.monoseq"/>
    <xsl:text> </xsl:text>
  </span>
</xsl:template>
<xsl:template match="tclcmdsynopsis/option">
  <u><xsl:apply-templates/></u>
</xsl:template>
<xsl:template match="tclcmdsynopsis/group">
  <xsl:if test="@choice='opt'">
    <xsl:text>?</xsl:text>
  </xsl:if>
  <xsl:apply-templates/>
  <xsl:if test="@rep='repeat'">
    <xsl:text>...</xsl:text>
  </xsl:if>
  <xsl:if test="@choice='opt'">
    <xsl:text>?</xsl:text>
  </xsl:if>
</xsl:template>
<xsl:template match="tclcmdsynopsis//arg[1]">
  <xsl:apply-templates/>
</xsl:template>
<xsl:template match="tclcmdsynopsis//arg[position() > 1]">
  <xsl:text> </xsl:text>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="tclcommand">
  <a href="#{translate(.,': ','__')}">
    <xsl:call-template name="inline.boldseq"/>
  </a>
</xsl:template>

<xsl:template match='tclpackage|tclnamespace'>
  <xsl:call-template name='inline.monoseq'/>
</xsl:template>

<xsl:template match="tclpkgsynopsis">
  <br/>
  <span class="{name(.)}">
    <pre>package require <xsl:value-of select="package"/> ?<xsl:value-of select="version"/>?</pre>
  </span>
</xsl:template>

<xsl:template match="tclnamespacesynopsis">
  <h3>Tcl Namespace Usage</h3>
  <xsl:apply-templates/>
  <p/>
</xsl:template>

<xsl:template match="tclnamespacesynopsis/tclnamespace[1]">
  <xsl:call-template name="inline.monoseq"/>
</xsl:template>

<xsl:template match="tclnamespacesynopsis/tclnamespace">
  <br/>
  <xsl:call-template name="inline.monoseq"/>
</xsl:template>

<xsl:template match="refsect1[title = 'Commands'][refsect2/title]//tclcmdsynopsis/*[position() = 1 and name() = 'option']">
  <tt>
    <xsl:choose>
      <xsl:when test="ancestor::refsect3//*[@role='subject']">
        <i><xsl:value-of select="ancestor::refsect3//*[@role='subject']"/></i>
      </xsl:when>
      <xsl:when test="ancestor::refsect2/title">
	<xsl:value-of select="ancestor::refsect2/title"/>
      </xsl:when>
      <xsl:otherwise/>
    </xsl:choose>
  </tt>
  <xsl:text> </xsl:text>
  <u><xsl:apply-templates/></u>
</xsl:template>

<xsl:template match="tcloptionsynopsis">
  <p>
    <xsl:apply-templates/>
  </p>
</xsl:template>

<xsl:template match="tcloptionsynopsis/option">
  <xsl:call-template name="inline.monoseq"/>
</xsl:template>

<xsl:template match="tcloptionsynopsis/arg">
  <u>
    <xsl:apply-templates/>
  </u>
</xsl:template>

<!-- Do a segmentedlist as a table, instead of the poxy way DocBook does them -->

<xsl:template match="segmentedlist">
  <table border="0">
    <xsl:apply-templates/>
  </table>
</xsl:template>

<xsl:template match="seglistitem">
  <tr>
    <xsl:apply-templates/>
  </tr>
</xsl:template>

<xsl:template match="seg">
  <td valign="top">
    <xsl:apply-templates/>
  </td>
</xsl:template>

<xsl:template match="seg/arg">
  <xsl:call-template name="inline.monoseq"/>
</xsl:template>

</xsl:stylesheet>
