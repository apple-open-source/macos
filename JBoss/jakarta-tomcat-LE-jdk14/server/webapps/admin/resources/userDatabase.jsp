<!-- Standard Struts Entries -->

<%@ page language="java" import="java.net.URLEncoder" contentType="text/html;charset=utf-8" %>
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

<html:form method="POST" action="/resources/saveUserDatabase">

  <html:hidden property="objectName"/>
  <html:hidden property="type"/>
  <html:hidden property="factory"/>

  <table width="100%" border="0" cellspacing="0" cellpadding="0">
    <tr class="page-title-row">
      <td align="left" nowrap>
        <div class="page-title-text">
          <bean:write name="userDatabaseForm" property="nodeLabel"/>
        </div>
      </td>
      <td align="right" nowrap>
        <div class="page-title-text">
          <controls:actions label="User Database Actions">
            <controls:action selected="true">
              ----<bean:message key="actions.available.actions"/>----
            </controls:action>
            <controls:action>
              ---------------------------------
            </controls:action>

            <controls:action url="/resources/setUpUserDatabase.do">
                <bean:message key="resources.actions.userdb.create"/>
            </controls:action>
            <controls:action url='<%= "/resources/listUserDatabases.do?forward="
                        + URLEncoder.encode("UserDatabases Delete List") %>'>
                <bean:message key="resources.actions.userdb.delete"/>
            </controls:action>
         </controls:actions>
        </div>
      </td>
    </tr>
  </table>

  <%@ include file="../buttons.jsp" %>
<br>

  <table border="0" cellspacing="0" cellpadding="0" width="100%">
    <tr><td><div class="table-title-text">
        <bean:message key="resources.treeBuilder.databases"/>
    </div></td></tr>
  </table>

  <table class="back-table" border="0" cellspacing="0" cellpadding="1" width="100%">
    <tr>
      <td>

        <controls:table tableStyle="front-table" lineStyle="line-row">
          <controls:row header="true"
                labelStyle="table-header-text" dataStyle="table-header-text">
            <controls:label><bean:message key="service.property"/></controls:label>
            <controls:data><bean:message key="service.value"/></controls:data>
          </controls:row>

          <controls:row labelStyle="table-label-text"
                         dataStyle="table-normal-text" styleId="name">
            <controls:label>
              <bean:message key="service.name"/>:
            </controls:label>
            <controls:data>
              <logic:present name="userDatabaseForm" property="objectName">
                <bean:write name="userDatabaseForm" property="name"/>
                <html:hidden property="name"/>
              </logic:present>
              <logic:notPresent name="userDatabaseForm" property="objectName">
                <html:text property="name" size="24" maxlength="32" styleId="name"/>
              </logic:notPresent>
            </controls:data>
          </controls:row>

          <controls:row labelStyle="table-label-text"
                         dataStyle="table-normal-text" styleId="location">
            <controls:label>
              <bean:message key="resources.userdb.location"/>:
            </controls:label>
            <controls:data>
              <html:text property="path" size="32" maxlength="64" styleId="location"/>
            </controls:data>
          </controls:row>

          <controls:row labelStyle="table-label-text"
                         dataStyle="table-normal-text" styleId="factory">
            <controls:label>
              <bean:message key="resources.userdb.factory"/>:
            </controls:label>
            <controls:data>
              <bean:write name="userDatabaseForm" property="factory"/>
            </controls:data>
          </controls:row>

          <controls:row labelStyle="table-label-text"
                         dataStyle="table-normal-text" styleId="description">
            <controls:label>
              <bean:message key="users.prompt.description"/>
            </controls:label>
            <controls:data>
              <html:textarea property="description" cols="32" rows="3" styleId="description"/>
            </controls:data>
          </controls:row>

        </controls:table>

      </td>

    </tr>

  </table>

  <%@ include file="../buttons.jsp" %>

</html:form>

<!-- Standard Footer -->

<%@ include file="../users/footer.jsp" %>

</body>

</html:html>
