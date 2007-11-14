<!-- Standard Struts Entries -->

<%@ page language="java" contentType="text/html;charset=utf-8" %>
<%@ taglib uri="/WEB-INF/struts-bean.tld" prefix="bean" %>
<%@ taglib uri="/WEB-INF/struts-html.tld" prefix="html" %>
<%@ taglib uri="/WEB-INF/struts-logic.tld" prefix="logic" %>

<html:html locale="true">

<!-- Standard Content -->

<%@ include file="header.jsp" %>

<!-- Body -->

<body bgcolor="white" background="images/PaperTexture.gif">

<center>

<h2>
  <bean:message key="error.login"/>
  <br>
  <bean:message key="error.tryagain"/>
  <html:link page="/">
    <bean:message key="error.here"/>
  </html:link>
</h2>

</center>

</body>

<!-- Standard Footer -->

<%@ include file="footer.jsp" %>

</html:html>
