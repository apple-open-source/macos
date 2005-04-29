<xsl:stylesheet version='1.0'
  xmlns:xsl='http://www.w3.org/1999/XSL/Transform'
  xmlns:doc="http://nwalsh.com/xsl/documentation/1.0"
  xmlns:str='http://xsltsl.org/string'
  extension-element-prefixes='str'
  exclude-result-prefixes="doc">

  <xsl:import href='xsltsl/stdlib.xsl'/>

  <doc:book xmlns=''>
    <title>Text Stylesheet</title>

    <para>This stylesheet produces a text rendition of a DocBook document.</para>
  </doc:book>

  <xsl:output method='text'/>

  <xsl:strip-space elements='*'/>
  <xsl:preserve-space elements='programlisting literallayout command'/>

  <xsl:template match='article'>
    <xsl:choose>
      <xsl:when test='title'>
        <xsl:apply-templates select='title'/>
      </xsl:when>
      <xsl:when test='articleinfo/title'>
        <xsl:apply-templates select='articleinfo/title'/>
      </xsl:when>
    </xsl:choose>

    <xsl:apply-templates select='author|articleinfo/author'/>
    <xsl:text>

</xsl:text>

    <xsl:apply-templates select='*[not(self::articleinfo)]'/>
  </xsl:template>

  <xsl:template match='article/title|articleinfo/title'>
    <xsl:text>
	</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>

</xsl:text>

    <xsl:if test='following-sibling::subtitle'>
      <xsl:text>	</xsl:text>
      <xsl:apply-templates select='following-sibling::subtitle'/>
      <xsl:if test='following-sibling::revhistory'>
        <xsl:text> Version </xsl:text>
        <xsl:apply-templates select='following-sibling::revhistory/revision[1]/revnumber'/>
      </xsl:if>
    </xsl:if>
    <xsl:text>

</xsl:text>
  </xsl:template>

  <xsl:template match='author'>
    <xsl:apply-templates select='firstname'/>
    <xsl:text> </xsl:text>
    <xsl:apply-templates select='surname'/>
    <xsl:if test='affiliation'>
      <xsl:text>, </xsl:text>
      <xsl:apply-templates select='affiliation/orgname'/>
    </xsl:if>
  </xsl:template>

  <xsl:template match='para'>
    <xsl:param name='indent' select='0'/>
    <xsl:param name='linelen' select='80'/>

    <xsl:call-template name='str:justify'>
      <xsl:with-param name='text'>
        <xsl:apply-templates/>
      </xsl:with-param>
      <xsl:with-param name='indent' select='$indent'/>
      <xsl:with-param name='max' select='$linelen'/>
    </xsl:call-template>
    <xsl:text>

</xsl:text>
  </xsl:template>

  <xsl:template match='note'>
    <xsl:call-template name='str:justify'>
      <xsl:with-param name='text'>
        <xsl:apply-templates/>
      </xsl:with-param>
      <xsl:with-param name='indent' select='4'/>
    </xsl:call-template>
    <xsl:text>

</xsl:text>
  </xsl:template>

  <xsl:template match='section'>
    <xsl:text>

</xsl:text>
    <xsl:apply-templates select='title'/>
    <xsl:if test='subtitle'>
      <xsl:text> (</xsl:text>
      <xsl:apply-templates select='subtitle'/>
      <xsl:text>)</xsl:text>
    </xsl:if>
    <xsl:text>
</xsl:text>
    <xsl:variable name='titlelen'>
      <xsl:choose>
        <xsl:when test='subtitle'>
          <xsl:value-of select='string-length(title) + 3 + string-length(subtitle)'/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select='string-length(title)'/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:call-template name='str:generate-string'>
      <xsl:with-param name='count' select='$titlelen'/>
      <xsl:with-param name='text'>
        <xsl:choose>
          <xsl:when test='parent::section'>-</xsl:when>
          <xsl:otherwise>=</xsl:otherwise>
        </xsl:choose>
      </xsl:with-param>
    </xsl:call-template>
    <xsl:text>

</xsl:text>

    <xsl:apply-templates select='*[not(self::title|self::subtitle|self::sectioninfo)]'/>
  </xsl:template>

  <xsl:template match='itemizedlist'>
    <xsl:apply-templates select='listitem'/>
    <xsl:text>
</xsl:text>
  </xsl:template>
  <xsl:template match='itemizedlist/listitem'>
    <xsl:call-template name='str:generate-string'>
      <xsl:with-param name='text' select='" "'/>
      <xsl:with-param name='count' select='count(ancestor::itemizedlist|ancestor::variablelist) * 4'/>
    </xsl:call-template>

    <xsl:text>* </xsl:text>
    <xsl:apply-templates select='*'>
      <xsl:with-param name='indent' select='count(ancestor::itemizedlist|ancestor::variablelist) * 4'/>
      <xsl:with-param name='linelen' select='80 - count(ancestor::itemizedlist|ancestor::variablelist) * 4'/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match='variablelist'>
    <xsl:apply-templates select='varlistentry'/>
  </xsl:template>
  <xsl:template match='varlistentry'>
    <xsl:call-template name='str:generate-string'>
      <xsl:with-param name='text' select='" "'/>
      <xsl:with-param name='count' select='(count(ancestor::variablelist|ancestor::itemizedlist) - 1) * 4'/>
    </xsl:call-template>

    <xsl:apply-templates select='term'/>
    <xsl:apply-templates select='listitem'/>
    <xsl:text>

</xsl:text>
  </xsl:template>
  <xsl:template match='varlistentry/term'>
    <xsl:apply-templates/>
    <xsl:text>
</xsl:text>
  </xsl:template>
  <xsl:template match='varlistentry/listitem'>
    <xsl:apply-templates select='*'>
      <xsl:with-param name='indent' select='count(ancestor::variablelist|ancestor::itemizedlist) * 4'/>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match='programlisting|literallayout'>
    <xsl:call-template name='indent'>
      <xsl:with-param name='text' select='.'/>
      <xsl:with-param name='indent' select='(count(ancestor::itemizedlist|ancestor::variablelist) + 1) * 4'/>
    </xsl:call-template>
    <xsl:text>

</xsl:text>
  </xsl:template>

  <xsl:template name='indent'>
    <xsl:param name='text'/>
    <xsl:param name='indent' select='4'/>

    <xsl:choose>
      <xsl:when test='not($text)'/>
      <xsl:when test='contains($text, "&#xa;")'>
        <xsl:call-template name='str:generate-string'>
          <xsl:with-param name='text' select='" "'/>
          <xsl:with-param name='count' select='$indent'/>
        </xsl:call-template>
        <xsl:value-of select='substring-before($text, "&#xa;")'/>
        <xsl:text>
</xsl:text>
        <xsl:call-template name='indent'>
          <xsl:with-param name='text' select='substring-after($text, "&#xa;")'/>
          <xsl:with-param name='indent' select='$indent'/>
        </xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <xsl:call-template name='str:generate-string'>
          <xsl:with-param name='text' select='" "'/>
          <xsl:with-param name='count' select='$indent'/>
        </xsl:call-template>
        <xsl:value-of select='$text'/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match='ulink'>
    <xsl:apply-templates/>
    <xsl:text> [</xsl:text>
    <xsl:value-of select='@url'/>
    <xsl:text>]</xsl:text>
  </xsl:template>

</xsl:stylesheet>
