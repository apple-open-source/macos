<!-- Standard Struts Entries -->
<%@ page language="java" contentType="text/html;charset=utf-8" %>
<%@ taglib uri="/WEB-INF/struts-bean.tld" prefix="bean" %>
<%@ taglib uri="/WEB-INF/struts-html.tld" prefix="html" %>
<%@ taglib uri="/WEB-INF/struts-logic.tld" prefix="logic" %>
<%@ taglib uri="/WEB-INF/controls.tld" prefix="controls" %>

<html:html locale="true">

<%@ include file="../users/header.jsp" %>

<!-- Body -->
<body bgcolor="white" background="../images/PaperTexture.gif">

<!--Form -->

<html:errors/>

<html:form method="POST" action="/DeleteAliases">

  <html:hidden property="hostName"/>
  <table width="100%" border="0" cellspacing="0" cellpadding="0">
    <tr bgcolor="7171A5">
      <td width="81%">
        <div class="page-title-text" align="left">
          <bean:message key="actions.alias.delete"/>
        </div>
      </td>
      <td align="right" nowrap>
        <div class="page-title-text">
            <controls:actions label="Alias Actions">
              <controls:action selected="true"> ----<bean:message key="actions.available.actions"/>---- </controls:action>
              <controls:action> --------------------------------- </controls:action>
            </controls:actions>
        </div>
      </td>
    </tr>
  </table>

<%@ include file="../buttons.jsp" %>
  <br>

  <%-- Aliases List --%>

  <table class="back-table" border="0" cellspacing="0" cellpadding="1" width="100%">
    <tr>
      <td>
        <table class="front-table" border="1"
         cellspacing="0" cellpadding="0" width="100%">
          <tr class="header-row">
            <td><div align="left" class="table-header-text">
              <bean:message key="actions.delete"/>
            </div></td>
            <td><div align="left" class="table-header-text">
              <bean:message key="host.alias.name"/>
            </div></td>
          </tr>

        <logic:iterate name="aliasesList" id="alias">
          <tr class="line-row">
            <td><div align="left" class="table-normal-text">&nbsp;
            <label for="aliases"></label>
              <html:multibox property="aliases"
                                value="<%= alias.toString() %>" styleId="aliases"/>
            </div></td>
            <td><div align="left" class="table-normal-text">&nbsp;
                <%= alias.toString() %>
            </div></td>
          </tr>
        </logic:iterate>
        </table>
      </td>
    </tr>
  </table>

<%@ include file="../buttons.jsp" %>

  <br>
</html:form>

<p>&nbsp;</p>
</body>
</html:html>
