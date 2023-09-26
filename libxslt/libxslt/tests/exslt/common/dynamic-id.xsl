<xsl:stylesheet
    version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:exsl="http://exslt.org/common">

<xsl:output indent="yes"/>

<xsl:template name="dynamic-id">
    <id>
        <xsl:value-of select="generate-id(exsl:node-set('string'))"/>
    </id>
</xsl:template>

<xsl:template match="/">
    <result>
        <xsl:call-template name="dynamic-id"/>
        <xsl:call-template name="dynamic-id"/>
        <xsl:call-template name="dynamic-id"/>
        <xsl:call-template name="dynamic-id"/>
        <xsl:call-template name="dynamic-id"/>
        <xsl:call-template name="dynamic-id"/>
        <xsl:call-template name="dynamic-id"/>
        <xsl:call-template name="dynamic-id"/>
        <xsl:call-template name="dynamic-id"/>
        <xsl:call-template name="dynamic-id"/>
    </result>
</xsl:template>

</xsl:stylesheet>
