<?xml version="1.0"?>

<!--
* JBoss, the OpenSource EJB server
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
-->

<!--
 Stylesheet to transform wl2jboss-tags.xml to html file.

 @author <a href="mailto:loubyansky@ukr.net">Alex Loubyansky</a>
-->

<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

   <xsl:template match="/">
      <xsl:element name="html">

         <xsl:element name="head">
            <xsl:element name="title">WebLogic to JBoss tags mapping</xsl:element>
         </xsl:element> <!-- head -->

         <xsl:element name="body">

            <!-- generate the contents-list first -->
            <xsl:call-template name="elements-list"/>

            <xsl:element name="table">
               <xsl:apply-templates select="weblogic-elements"/>
            </xsl:element> <!-- table -->

         </xsl:element> <!-- body -->

      </xsl:element> <!-- html -->
   </xsl:template>

   <xsl:template match="weblogic-elements">
      <xsl:for-each select="./weblogic-element">
         <xsl:element name="tr">
            <xsl:element name="td">
               <xsl:apply-templates select="."/>
               <xsl:element name="hr"/>
            </xsl:element> <!-- td -->
         </xsl:element> <!-- tr -->
      </xsl:for-each>
   </xsl:template>

   <xsl:template match="weblogic-element">

      <xsl:element name="table">

         <xsl:attribute name="width"><xsl:text>100%</xsl:text></xsl:attribute>
         <xsl:attribute name="border"><xsl:text>1</xsl:text></xsl:attribute>
         <xsl:attribute name="cellspacing"><xsl:text>0</xsl:text></xsl:attribute>
         <xsl:attribute name="cellpadding"><xsl:text>3</xsl:text></xsl:attribute>

         <!-- ELEMENT NAME -->
         <xsl:element name="tr">
            <xsl:attribute name="valign"><xsl:text>top</xsl:text></xsl:attribute>

            <xsl:element name="th">Element name:</xsl:element> <!-- th -->
            <xsl:element name="td">

               <!-- make a link to the element -->
               <xsl:element name="a">
                  <xsl:attribute name="name">
                     <xsl:value-of select="element-name"/>
                  </xsl:attribute> <!-- name -->
                  <xsl:value-of select="element-name"/>
               </xsl:element> <!-- a -->

            </xsl:element> <!-- td -->
         </xsl:element>

         <!-- SINCE VERSION -->
         <xsl:element name="tr">
            <xsl:attribute name="valign"><xsl:text>top</xsl:text></xsl:attribute>

            <xsl:element name="th">Since version:</xsl:element> <!-- th -->
            <xsl:element name="td">
               <xsl:choose>
                  <xsl:when test="./since-version">
                     <xsl:value-of select="since-version"/>
                  </xsl:when>
                  <xsl:otherwise>n/a</xsl:otherwise>
               </xsl:choose>
            </xsl:element> <!-- td -->
         </xsl:element> <!-- tr -->

         <!-- RANGE OF VALUES -->
         <xsl:element name="tr">
            <xsl:attribute name="valign"><xsl:text>top</xsl:text></xsl:attribute>

            <xsl:element name="th">Range of values:</xsl:element> <!-- th -->
            <xsl:element name="td">
               <xsl:choose>
                  <xsl:when test="./range-of-values/value">
                     <xsl:for-each select="./range-of-values/value">
                        <xsl:value-of select="."/>
                        <xsl:if test="(position() &lt; last())">, </xsl:if>
                     </xsl:for-each> <!-- value -->
                  </xsl:when>
                  <xsl:otherwise>n/a</xsl:otherwise>
               </xsl:choose>
            </xsl:element> <!-- td -->
         </xsl:element> <!-- tr -->

         <!-- DEFAULT VALUE -->
         <xsl:element name="tr">
            <xsl:attribute name="valign"><xsl:text>top</xsl:text></xsl:attribute>

            <xsl:element name="th">Default value:</xsl:element> <!-- th -->
            <xsl:element name="td">
               <xsl:choose>
                  <xsl:when test="./default-value">
                     <xsl:value-of select="default-value"/>
                  </xsl:when>
                  <xsl:otherwise>n/a</xsl:otherwise>
               </xsl:choose>
            </xsl:element> <!-- td -->
         </xsl:element> <!-- tr -->

         <!-- REQUIREMENTS -->
         <xsl:element name="tr">
            <xsl:attribute name="valign"><xsl:text>top</xsl:text></xsl:attribute>

            <xsl:element name="th">Requirements:</xsl:element> <!-- th -->
            <xsl:element name="td">
               <xsl:choose>
                  <xsl:when test="./requirements">
                     <xsl:value-of select="requirements"/>
                  </xsl:when>
                  <xsl:otherwise>n/a</xsl:otherwise>
               </xsl:choose>
            </xsl:element> <!-- td -->
         </xsl:element> <!-- tr -->

         <!-- PARENT ELEMENTS -->
         <xsl:element name="tr">
            <xsl:attribute name="valign"><xsl:text>top</xsl:text></xsl:attribute>

            <xsl:element name="th">Parent elements:</xsl:element> <!-- th -->
            <xsl:element name="td">
               <xsl:for-each select="./parent-elements/element-name">
                  <xsl:value-of select="."/>
                  <xsl:if test="(position() &lt; last())">, </xsl:if>
               </xsl:for-each> <!-- element-name -->
            </xsl:element> <!-- td -->
         </xsl:element> <!-- tr -->

         <!-- DEPLOYMENT FILE -->
         <xsl:element name="tr">
            <xsl:attribute name="valign"><xsl:text>top</xsl:text></xsl:attribute>

            <xsl:element name="th">Deployment file:</xsl:element> <!-- th -->
            <xsl:element name="td">
               <xsl:value-of select="deployment-file"/>
            </xsl:element> <!-- td -->
         </xsl:element> <!-- tr -->

         <!-- FUNCTION -->
         <xsl:element name="tr">
            <xsl:attribute name="valign"><xsl:text>top</xsl:text></xsl:attribute>

            <xsl:element name="th">Function:</xsl:element> <!-- th -->
            <xsl:element name="td">
               <xsl:value-of select="function"/>
            </xsl:element> <!-- td -->
         </xsl:element> <!-- tr -->

         <!-- EXAMPLES -->
         <xsl:element name="tr">
            <xsl:attribute name="valign"><xsl:text>top</xsl:text></xsl:attribute>

            <xsl:element name="th">Examples:</xsl:element> <!-- th -->
            <xsl:element name="td">
               <xsl:element name="table">
                  <xsl:for-each select="./examples/example">
                     <xsl:element name="tr">
                        <xsl:element name="td">
                           <xsl:value-of select="."/>
                        </xsl:element> <!-- td -->
                     </xsl:element> <!-- tr -->
                  </xsl:for-each> <!-- example -->
               </xsl:element> <!-- table -->

               <xsl:if test="./examples/example-ref">See </xsl:if>
               <xsl:for-each select="./examples/example-ref">
                  <!-- make an anchor -->
                  <xsl:element name="a">
                     <xsl:attribute name="href">
                        <xsl:text>#</xsl:text>
                        <xsl:value-of select="element-name"/>
                     </xsl:attribute> <!-- href -->
                     <xsl:value-of select="element-name"/>
                  </xsl:element> <!-- a -->

                  <xsl:if test="(position() &lt; last())">, </xsl:if>
               </xsl:for-each> <!-- example-ref -->

            </xsl:element> <!-- td -->
         </xsl:element> <!-- tr -->

         <!-- JBOSS ELEMENTS -->
         <xsl:element name="tr">
            <xsl:attribute name="valign"><xsl:text>top</xsl:text></xsl:attribute>

            <xsl:element name="th">JBoss elements:</xsl:element> <!-- th -->
            <xsl:element name="td">
               <xsl:element name="table">

                  <xsl:attribute name="width"><xsl:text>100%</xsl:text></xsl:attribute>
                  <xsl:attribute name="border"><xsl:text>1</xsl:text></xsl:attribute>
                  <xsl:attribute name="cellspacing"><xsl:text>0</xsl:text></xsl:attribute>
                  <xsl:attribute name="cellpadding"><xsl:text>3</xsl:text></xsl:attribute>

                  <xsl:choose>
                     <xsl:when test="./jboss-elements/jboss-element">
                        <xsl:element name="tr">
                           <xsl:element name="th">Name</xsl:element>
                           <xsl:element name="th">Deployment file</xsl:element>
                        </xsl:element> <!-- tr -->
                     </xsl:when>
                     <xsl:otherwise>
                        not mapped
                     </xsl:otherwise>
                  </xsl:choose>

                  <xsl:for-each select="./jboss-elements/jboss-element">
                     <xsl:element name="tr">
                        <xsl:element name="td">
                           <xsl:value-of select="./element-name"/>
                        </xsl:element> <!-- td -->
                        <xsl:element name="td">
                           <xsl:value-of select="./deployment-file"/>
                        </xsl:element> <!-- td -->
                     </xsl:element> <!-- tr -->
                  </xsl:for-each> <!-- element-name -->

               </xsl:element> <!-- table -->
            </xsl:element> <!-- td -->
         </xsl:element> <!-- tr -->

      </xsl:element> <!-- table -->

      <!-- make an anchor to the top -->
      <xsl:element name="a">
         <xsl:attribute name="href">
            <xsl:text>#top</xsl:text>
         </xsl:attribute> <!-- href -->
         <xsl:text>top</xsl:text>
      </xsl:element> <!-- a -->

   </xsl:template> <!-- weblogic-element -->

   <!-- lists all elements with links -->
   <xsl:template name="elements-list">

      <!-- make a link to the contents -->
      <xsl:element name="a">
         <xsl:attribute name="name">
            <xsl:text>top</xsl:text>
         </xsl:attribute> <!-- name -->
            <xsl:element name="h3">
               <xsl:text>Contents</xsl:text>
            </xsl:element> <!-- h3 -->
      </xsl:element> <!-- a -->

      <xsl:element name="table">
         <xsl:for-each select="//weblogic-elements/weblogic-element">
            <xsl:element name="tr">
               <xsl:element name="td">

                  <!-- make an anchor -->
                  <xsl:element name="a">
                     <xsl:attribute name="href">
                        <xsl:text>#</xsl:text>
                        <xsl:value-of select="element-name"/>
                     </xsl:attribute> <!-- href -->
                     <xsl:value-of select="element-name"/>
                  </xsl:element> <!-- a -->

               </xsl:element> <!-- td -->
            </xsl:element> <!-- tr -->
         </xsl:for-each> <!-- weblogic-element -->
      </xsl:element> <!-- table -->

      <xsl:element name="hr"/>
   </xsl:template> <!-- elements-list -->

</xsl:stylesheet>
