<?xml version="1.0"?>

<!--
   - nroff.xsl -
   -
   - Copyright (c) 2000 Zveno Pty Ltd
   -
   -	XSLT stylesheet to convert DocBook+Tcl mods to nroff.
   -	NB. Tcl man macros are used.
   -
   - $Id: nroff.xsl,v 1.3 2002/11/11 22:45:19 balls Exp $
   -->

<xsl:stylesheet version='1.0'
  xmlns:xsl='http://www.w3.org/1999/XSL/Transform'>

<xsl:output method="text"/>

<xsl:variable name="tclversion" select='""'/>
<xsl:variable name="maxlinelen" select="60"/>

<xsl:template match="refentry">'\"
<xsl:apply-templates select="docinfo"/>
'\" 
'\" RCS: @(#) $Id: nroff.xsl,v 1.3 2002/11/11 22:45:19 balls Exp $
'\" 
'\" The definitions below are for supplemental macros used in Tcl/Tk
'\" manual entries.
'\"
'\" .AP type name in/out ?indent?
'\"	Start paragraph describing an argument to a library procedure.
'\"	type is type of argument (int, etc.), in/out is either "in", "out",
'\"	or "in/out" to describe whether procedure reads or modifies arg,
'\"	and indent is equivalent to second arg of .IP (shouldn't ever be
'\"	needed;  use .AS below instead)
'\"
'\" .AS ?type? ?name?
'\"	Give maximum sizes of arguments for setting tab stops.  Type and
'\"	name are examples of largest possible arguments that will be passed
'\"	to .AP later.  If args are omitted, default tab stops are used.
'\"
'\" .BS
'\"	Start box enclosure.  From here until next .BE, everything will be
'\"	enclosed in one large box.
'\"
'\" .BE
'\"	End of box enclosure.
'\"
'\" .CS
'\"	Begin code excerpt.
'\"
'\" .CE
'\"	End code excerpt.
'\"
'\" .VS ?version? ?br?
'\"	Begin vertical sidebar, for use in marking newly-changed parts
'\"	of man pages.  The first argument is ignored and used for recording
'\"	the version when the .VS was added, so that the sidebars can be
'\"	found and removed when they reach a certain age.  If another argument
'\"	is present, then a line break is forced before starting the sidebar.
'\"
'\" .VE
'\"	End of vertical sidebar.
'\"
'\" .DS
'\"	Begin an indented unfilled display.
'\"
'\" .DE
'\"	End of indented unfilled display.
'\"
'\" .SO
'\"	Start of list of standard options for a Tk widget.  The
'\"	options follow on successive lines, in four columns separated
'\"	by tabs.
'\"
'\" .SE
'\"	End of list of standard options for a Tk widget.
'\"
'\" .OP cmdName dbName dbClass
'\"	Start of description of a specific option.  cmdName gives the
'\"	option's name as specified in the class command, dbName gives
'\"	the option's name in the option database, and dbClass gives
'\"	the option's class in the option database.
'\"
'\" .UL arg1 arg2
'\"	Print arg1 underlined, then print arg2 normally.
'\"
'\" RCS: @(#) $Id: nroff.xsl,v 1.3 2002/11/11 22:45:19 balls Exp $
'\"
'\"	# Set up traps and other miscellaneous stuff for Tcl/Tk man pages.
.if t .wh -1.3i ^B
.nr ^l \n(.l
.ad b
'\"	# Start an argument description
.de AP
.ie !"\\$4"" .TP \\$4
.el \{\
.   ie !"\\$2"" .TP \\n()Cu
.   el          .TP 15
.\}
.ta \\n()Au \\n()Bu
.ie !"\\$3"" \{\
\&amp;\\$1	\\fI\\$2\\fP	(\\$3)
.\".b
.\}
.el \{\
.br
.ie !"\\$2"" \{\
\&amp;\\$1	\\fI\\$2\\fP
.\}
.el \{\
\&amp;\\fI\\$1\\fP
.\}
.\}
..
'\"	# define tabbing values for .AP
.de AS
.nr )A 10n
.if !"\\$1"" .nr )A \\w'\\$1'u+3n
.nr )B \\n()Au+15n
.\"
.if !"\\$2"" .nr )B \\w'\\$2'u+\\n()Au+3n
.nr )C \\n()Bu+\\w'(in/out)'u+2n
..
.AS Tcl_Interp Tcl_CreateInterp in/out
'\"	# BS - start boxed text
'\"	# ^y = starting y location
'\"	# ^b = 1
.de BS
.br
.mk ^y
.nr ^b 1u
.if n .nf
.if n .ti 0
.if n \l'\\n(.lu\(ul'
.if n .fi
..
'\"	# BE - end boxed text (draw box now)
.de BE
.nf
.ti 0
.mk ^t
.ie n \l'\\n(^lu\(ul'
.el \{\
.\"	Draw four-sided box normally, but don't draw top of
.\"	box if the box started on an earlier page.
.ie !\\n(^b-1 \{\
\h'-1.5n'\L'|\\n(^yu-1v'\l'\\n(^lu+3n\(ul'\L'\\n(^tu+1v-\\n(^yu'\l'|0u-1.5n\(ul'
.\}
.el \}\
\h'-1.5n'\L'|\\n(^yu-1v'\h'\\n(^lu+3n'\L'\\n(^tu+1v-\\n(^yu'\l'|0u-1.5n\(ul'
.\}
.\}
.fi
.br
.nr ^b 0
..
'\"	# VS - start vertical sidebar
'\"	# ^Y = starting y location
'\"	# ^v = 1 (for troff;  for nroff this doesn't matter)
.de VS
.if !"\\$2"" .br
.mk ^Y
.ie n 'mc \s12\(br\s0
.el .nr ^v 1u
..
'\"	# VE - end of vertical sidebar
.de VE
.ie n 'mc
.el \{\
.ev 2
.nf
.ti 0
.mk ^t
\h'|\\n(^lu+3n'\L'|\\n(^Yu-1v\(bv'\v'\\n(^tu+1v-\\n(^Yu'\h'-|\\n(^lu+3n'
.sp -1
.fi
.ev
.\}
.nr ^v 0
..
'\"	# Special macro to handle page bottom:  finish off current
'\"	# box/sidebar if in box/sidebar mode, then invoked standard
'\"	# page bottom macro.
.de ^B
.ev 2
'ti 0
'nf
.mk ^t
.if \\n(^b \{\
.\"	Draw three-sided box if this is the box's first page,
.\"	draw two sides but no top otherwise.
.ie !\\n(^b-1 \h'-1.5n'\L'|\\n(^yu-1v'\l'\\n(^lu+3n\(ul'\L'\\n(^tu+1v-\\n(^yu'\h'|0u'\c
.el \h'-1.5n'\L'|\\n(^yu-1v'\h'\\n(^lu+3n'\L'\\n(^tu+1v-\\n(^yu'\h'|0u'\c
.\}
.if \\n(^v \{\
.nr ^x \\n(^tu+1v-\\n(^Yu
\kx\h'-\\nxu'\h'|\\n(^lu+3n'\ky\L'-\\n(^xu'\v'\\n(^xu'\h'|0u'\c
.\}
.bp
'fi
.ev
.if \\n(^b \{\
.mk ^y
.nr ^b 2
.\}
.if \\n(^v \{\
.mk ^Y
.\}
..
'\"	# DS - begin display
.de DS
.RS
.nf
.sp
..
'\"	# DE - end display
.de DE
.fi
.RE
.sp
..
'\"	# SO - start of list of standard options
.de SO
.SH "STANDARD OPTIONS"
.LP
.nf
.ta 4c 8c 12c
.ft B
..
'\"	# SE - end of list of standard options
.de SE
.fi
.ft R
.LP
See the \\fBoptions\\fR manual entry for details on the standard options.
..
'\"	# OP - start of full description for a single option
.de OP
.LP
.nf
.ta 4c
Command-Line Name:	\\fB\\$1\\fR
Database Name:	\\fB\\$2\\fR
Database Class:	\\fB\\$3\\fR
.fi
.IP
..
'\"	# CS - begin code excerpt
.de CS
.RS
.nf
.ta .25i .5i .75i 1i
..
'\"	# CE - end code excerpt
.de CE
.fi
.RE
..
.de UL
\\$1\l'|0\(ul'\\$2
..
<xsl:apply-templates select="refmeta"/>
.BS
<xsl:apply-templates select="*[name() != 'docinfo' and name() != 'refmeta']"/>
</xsl:template>

  <xsl:template match="docinfo|docinfo/legalnotice">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="docinfo/copyright">
    <xsl:text>'\" Copyright (c) </xsl:text><xsl:value-of select="year"/><xsl:text> </xsl:text><xsl:value-of select="holder"/><xsl:text>
'\"
</xsl:text>
  </xsl:template>

  <!-- We need a way of finding the last space char on each line,
     - trimming the line at that point and then continuing with
     - string after that space char.  It's in the "too-hard" basket for now.
     -
     - SRB 20021030: xsltsl should provide this functionality.
    -->

  <xsl:template match="docinfo/legalnotice/para[1]">
    <xsl:call-template name="comment.line">
      <xsl:with-param name="line" select="substring(.,1,$maxlinelen)"/>
    </xsl:call-template>
    <xsl:call-template name="comment.line.continue">
      <xsl:with-param name="text" select="substring(.,$maxlinelen + 1)"/>
    </xsl:call-template>
  </xsl:template>
<xsl:template match="docinfo/legalnotice/para[position() > 1]">
'\"
<xsl:call-template name="comment.line">
  <xsl:with-param name="line" select="substring(.,1,$maxlinelen)"/>
</xsl:call-template>
<xsl:call-template name="comment.line.continue">
  <xsl:with-param name="text" select="substring(.,$maxlinelen + 1)"/>
</xsl:call-template>
</xsl:template>

<xsl:template name="comment.line">
  <xsl:param name="line"/>
  <xsl:text>'\" </xsl:text>
  <xsl:value-of select="$line"/>
</xsl:template>
<xsl:template name="comment.line.continue">
  <xsl:param name="text"/>
  <xsl:choose>
    <xsl:when test="string-length($text) > 0">
      <xsl:text>
</xsl:text>
      <xsl:call-template name="comment.line">
	<xsl:with-param name="line" select="substring($text,1,$maxlinelen)"/>
      </xsl:call-template>
      <xsl:call-template name="comment.line.continue">
	<xsl:with-param name="text" select="substring($text,$maxlinelen + 1)"/>
      </xsl:call-template>
    </xsl:when>
    <xsl:otherwise/>
  </xsl:choose>
</xsl:template>

<xsl:template match="refmeta">
  <xsl:text>.TH </xsl:text>
  <xsl:value-of select="refentrytitle"/>
  <xsl:text> </xsl:text>
  <xsl:value-of select="manvolnum"/>
  <xsl:text> </xsl:text>
  <xsl:value-of select="$tclversion"/>
  <xsl:text> Tcl "Tcl Built-In Commands"</xsl:text>
</xsl:template>

<xsl:template match="refnamediv">
  <xsl:text>'\" Note:  do not modify the .SH NAME line immediately below!
.SH NAME
</xsl:text>
  <xsl:value-of select="refname[1]"/>
  <xsl:text> \- </xsl:text>
  <xsl:value-of select="refpurpose"/>
  <xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="refsynopsisdiv">
  <xsl:text>.SH SYNOPSIS
</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>
.BE
</xsl:text>
</xsl:template>

<xsl:template match="tclcmdsynopsis|tcloptionsynopsis|tclpkgsynopsis|tclnamespacesynopsis">
  <xsl:apply-templates/>
  <xsl:choose>
    <xsl:when test="command/node()[position() = last() and self::*]">
      <!-- last element would have reset font -->
    </xsl:when>
    <xsl:otherwise>
      <xsl:text>\fP</xsl:text>
    </xsl:otherwise>
  </xsl:choose>
  <xsl:text>
.sp
</xsl:text>
</xsl:template>
<xsl:template match="tclcmdsynopsis[position() = last()]|tcloptionsynopsis[position() = last()]">
  <xsl:apply-templates/>
  <xsl:choose>
    <xsl:when test="command/node()[position() = last() and self::*]">
      <!-- last element would have reset font -->
    </xsl:when>
    <xsl:otherwise>
      <xsl:text>\fP</xsl:text>
    </xsl:otherwise>
  </xsl:choose>
</xsl:template>

<xsl:template match="tclcmdsynopsis/command">
  <xsl:text>\fB</xsl:text>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="option">
  <xsl:text>\fI </xsl:text>
  <xsl:apply-templates/>
  <xsl:text>\fR</xsl:text>
</xsl:template>

<xsl:template match="group">
  <xsl:apply-templates/>
</xsl:template>
<xsl:template match="group[@choice='opt']">
  <xsl:text> ?</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>?</xsl:text>
</xsl:template>
<xsl:template match="group[@choice='opt' and @rep='repeat']">
  <xsl:text> ?</xsl:text>
  <xsl:apply-templates/>
  <xsl:text> ... ?</xsl:text>
</xsl:template>

<xsl:template match="arg">
  <xsl:text>\fI </xsl:text>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="replaceable">
  <xsl:text>\fI</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>\fR</xsl:text>
</xsl:template>

  <xsl:template match="tclcommand|tclnamespace">
    <xsl:call-template name='inline.bold'/>
  </xsl:template>
  <xsl:template name='inline.bold'>
    <xsl:text>\fB</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>\fR</xsl:text>
  </xsl:template>

<xsl:template match="refsect1">
  <xsl:text>
.SH </xsl:text>
  <xsl:value-of select="translate(title,'abcdefghijklmnopqrstuvwxyz','ABCDEFGHIJKLMNOPQRSTUVWXYZ')"/>
  <xsl:text>
</xsl:text>
  <xsl:apply-templates select='*[not(self::title)]'/>
</xsl:template>

<xsl:template match="para">
  <xsl:text>
.PP
</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="para/text()">
  <xsl:value-of select="."/>
</xsl:template>

  <xsl:template match="refsect2">
    <xsl:apply-templates select="para|title"/>
    <xsl:text>
.RS
</xsl:text>
    <xsl:apply-templates select="refsect3"/>
    <xsl:text>
.RE
</xsl:text>
  </xsl:template>

  <xsl:template match="refsect3">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="acronym">
    <xsl:apply-templates/>
  </xsl:template>

<xsl:template match="refsect2/title|refsect3/title">
  <xsl:text>.TP
</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="refsect1[title = 'Commands'][refsect2/title]//tclcmdsynopsis/*[position() = 1 and name() = 'option']">
  <xsl:choose>
    <xsl:when test="ancestor::refsect3//*[@role='subject']">
      <xsl:text>.TP
\fI</xsl:text>
      <xsl:value-of select="ancestor::refsect3//*[@role='subject']"/>
      <xsl:text>\fR</xsl:text>
    </xsl:when>
    <xsl:when test="ancestor::refsect2/title">
      <xsl:text>.TP
</xsl:text>
      <xsl:value-of select="ancestor::refsect2/title"/>
    </xsl:when>
    <xsl:otherwise/>
  </xsl:choose>
  <xsl:text> </xsl:text>
  <xsl:apply-templates/>
</xsl:template>

  <xsl:template match="segmentedlist|variablelist">
    <xsl:text>
.TP
.RS
</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>.RE
</xsl:text>
  </xsl:template>

  <xsl:template match="seglistitem">
    <xsl:text>.TP
\fI</xsl:text>
    <xsl:value-of select="seg[1]"/>
    <xsl:text>\fP </xsl:text>
    <xsl:value-of select="seg[2]"/>
    <xsl:text>
</xsl:text>
  </xsl:template>

  <xsl:template match="varlistentry">
    <xsl:text>.TP
\fI</xsl:text>
    <xsl:apply-templates select="term"/>
    <xsl:text>\fP </xsl:text>
    <xsl:apply-templates select="listitem"/>
    <xsl:text>
</xsl:text>
  </xsl:template>
  <xsl:template match='term'>
    <xsl:call-template name='inline.bold'/>
  </xsl:template>
  <xsl:template match='listitem'>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match='methodname|version|package'>
    <xsl:call-template name='inline.bold'/>
  </xsl:template>

<xsl:template match="informalexample">
  <xsl:text>.PP
</xsl:text>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="programlisting">
  <xsl:text>.CS
</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>
.CE
</xsl:text>
</xsl:template>

  <xsl:template match='author'>
    <xsl:apply-templates select='firstname'/>
    <xsl:text> </xsl:text>
    <xsl:apply-templates select='surname'/>
  </xsl:template>

  <xsl:template match='firstname|surname'>
    <xsl:apply-templates/>
  </xsl:template>

<!-- Override built-in rules to omit unwanted content -->

<xsl:template match="*">
  <xsl:message>No template matching <xsl:value-of select="name()"/> (parent <xsl:value-of select="name(..)"/>)</xsl:message>
</xsl:template>
<xsl:template match="text()[string-length(normalize-space()) = 0]|@*">
  <!-- Don't emit white space -->
</xsl:template>

</xsl:stylesheet>
