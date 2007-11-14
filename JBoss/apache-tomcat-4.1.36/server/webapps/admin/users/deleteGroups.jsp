<!-- Standard Struts Entries -->
<%@ page language="java" import="java.net.URLEncoder" contentType="text/html;charset=utf-8" %>
<%@ taglib uri="/WEB-INF/struts-bean.tld" prefix="bean" %>
<%@ taglib uri="/WEB-INF/struts-html.tld" prefix="html" %>
<%@ taglib uri="/WEB-INF/struts-logic.tld" prefix="logic" %>
<%@ taglib uri="/WEB-INF/controls.tld" prefix="controls" %>

<html:html locale="true">

<%@ include file="header.jsp" %>

<!-- Body -->
<body bgcolor="white" background="../images/PaperTexture.gif">

<!--Form -->

<html:errors/>

<html:form action="/users/listGroups">

  <table width="100%" border="0" cellspacing="0" cellpadding="0">
    <tr bgcolor="7171A5">
      <td width="81%">
        <div class="page-title-text" align="left">
          <bean:message key="users.deleteGroups.title"/>
        </div>
      </td>
      <td width="19%">
        <div align="right">
          <%@ include file="listGroups.jspf" %>
        </div>
      </td>
    </tr>
  </table>

</html:form>

<br>
<bean:define id="checkboxes" scope="page" value="true"/>
<html:form action="/users/deleteGroups">
  <%@ include file="../buttons.jsp" %>
  <br>
  <html:hidden property="databaseName"/>
  <%@ include file="groups.jspf" %>
  <%@ include file="../buttons.jsp" %>
</html:form>
<br>

<%@ include file="footer.jsp" %>

</body>
</html:html>
