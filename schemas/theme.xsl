<?xml version="1.0" encoding="UTF-8" ?>
<!-- 
    Stylesheet to generate our THEME API in C

    Author: Luis Mondesi <lemsx1@gmail.com> 
    Date: 2006-04-05 02:02 EDT 
    Usage: xsltproc theme.xsl theme.xsd > theme.c

    This Stylesheet is made available under the terms of the GNU GPL.

    See the file COPYING at the root of the source repository, or 
    http://www.gnu.org/ for details.

    FIXME we are only sorting for now ...
-->
<xsl:transform version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform" 
    xmlns:xs="http://www.w3.org/2001/XMLSchema">

    <xsl:template match="*|text()|@*">
        <xsl:copy>
            <xsl:apply-templates select="*|text()|@*">
                <xsl:sort select="@*" />
            </xsl:apply-templates>
        </xsl:copy>
    </xsl:template>

</xsl:transform>
