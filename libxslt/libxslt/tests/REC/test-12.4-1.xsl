<xsl:stylesheet
    version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:d="s:def"
    exclude-result-prefixes="d">

<xsl:output indent="yes"/>

<xsl:template match="/">
    <result>
        <document>
            <xsl:value-of select="generate-id(/)"/>
        </document>
        <element>
            <xsl:value-of select="generate-id(/d:doc/d:elem)"/>
        </element>
        <attribute>
            <xsl:value-of select="generate-id(d:doc/d:elem/@attr)"/>
        </attribute>
        <namespace>
            <xsl:value-of select="generate-id(d:doc/d:elem/namespace::*[local-name()=''])"/>
        </namespace>
        <namespace>
            <xsl:value-of select="generate-id(d:doc/d:elem/namespace::äöü)"/>
        </namespace>
        <text>
            <xsl:value-of select="generate-id(d:doc/d:text/text())"/>
        </text>
        <comment>
            <xsl:value-of select="generate-id(d:doc/comment())"/>
        </comment>
        <processing-instruction>
            <xsl:value-of select="generate-id(d:doc/processing-instruction())"/>
        </processing-instruction>
    </result>
</xsl:template>

</xsl:stylesheet>
