<!-- Standard Struts Entries -->

<%@ page language="java" contentType="text/html;charset=utf-8" %>
<%@ taglib uri="/WEB-INF/struts-bean.tld" prefix="bean" %>
<%@ taglib uri="/WEB-INF/struts-html.tld" prefix="html" %>
<%@ taglib uri="/WEB-INF/struts-logic.tld" prefix="logic" %>

<html:html locale="true">

<!-- Standard Content -->

<%@ include file="header.jsp" %>

<!-- Body -->

<body leftmargin="0" topmargin="0" marginwidth="0" marginheight="0" bgcolor="7171A5" background="images/BlueTile.gif">

<table width="100%" border="0" cellspacing="0" cellpadding="0">
    <tr>
      <td align="left" valign="middle">
        <div class="masthead-title-text" align="left"><img src="images/TomcatBanner.jpg" alt="Tomcat Web Server Administration Tool" height="120"></div>
      </td>
      <form method='post' action='commitChanges.do' target='_self'>
      <td align="right" valign="middle">
        <html:submit>
          <bean:message key="button.commit"/>
        </html:submit>
      </td>
      </form>
      <td width="1%">
        <div class="table-normal-text" align="left">&nbsp </div>
      </td>
    <form method='post' action='logOut.do' target='_top'>
      <td align="right" valign="middle">
        <html:submit>
          <bean:message key="button.logout"/>
        </html:submit>
      </td>
      <td width="1%">
        <div class="table-normal-text" align="left">&nbsp </div>
      </td>
    </form>
  </tr>
</table>

<!-- Select language -->
<!--

<h2><bean:message key="login.changeLanguage"/></h2>

<html:form action="/setLocale" method="POST" target="_self">
  <table border="0" cellspacing="5">
    <tr>
      <td align="right">
        <html:select property="locale">
          <html:options name="applicationLocales"
                    property="localeValues"
                   labelName="applicationLocales"
               labelProperty="localeLabels"/>
        </html:select>
      </td>
      <td align="left">
        <html:submit>
          <bean:message key="button.change"/>
        </html:submit>
      </td>
    </tr>
  </table>
</html:form>
-->

</body>

<!-- Standard Footer -->

<%@ include file="footer.jsp" %>

</html:html>
