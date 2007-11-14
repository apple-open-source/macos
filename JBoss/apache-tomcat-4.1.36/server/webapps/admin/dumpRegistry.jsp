<!-- Standard Struts Entries -->

<%@ page language="java" contentType="text/html;charset=utf-8" %>
<%@ taglib uri="/WEB-INF/struts-bean.tld" prefix="bean" %>
<%@ taglib uri="/WEB-INF/struts-html.tld" prefix="html" %>
<%@ taglib uri="/WEB-INF/struts-logic.tld" prefix="logic" %>

<html:html locale="true">

<!-- Standard Content -->

<%@ include file="header.jsp" %>

<!-- Body -->

<body bgcolor="white">

<table border="1" cellpadding="5">

  <tr>
    <th align="center" colspan="3">
      Registered Managed Beans
    </th>
  </tr>

  <tr>
    <th align="center">Name</th>
    <th align="center">Group</th>
    <th align="center">Description</th>
  </tr>

  <logic:iterate id="bean" name="beans">
    <tr>
      <td><bean:write name="bean" property="name"/></td>
      <td><bean:write name="bean" property="group"/></td>
      <td><bean:write name="bean" property="description"/></td>
    </tr>
  </logic:iterate>

</table>

</body>

<!-- Standard Footer -->

<%@ include file="footer.jsp" %>

</html:html>
