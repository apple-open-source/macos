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

<html:form method="POST" action="/users/saveRole"> <!--focus="rolename"-->

  <html:hidden property="databaseName"/>
  <html:hidden property="objectName"/>

  <table width="100%" border="0" cellspacing="0" cellpadding="0">
    <tr class="page-title-row">
      <td align="left" nowrap>
        <div class="page-title-text">
          <bean:write name="roleForm" property="nodeLabel"/>
        </div>
      </td>
      <td align="right" nowrap>
        <div class="page-title-text">
          <controls:actions label="Role Actions">
            <controls:action selected="true">
              ----<bean:message key="actions.available.actions"/>----
            </controls:action>
            <controls:action>
              ---------------------------------
            </controls:action>
            <!-- will add the urls later once those screens get implemented -->
<%--
            <controls:action url="">
              <bean:message key="users.actions.role.create"/>
            </controls:action>
            <controls:action url="">
              <bean:message key="users.actions.role.delete"/>
            </controls:action>
--%>
          </controls:actions>
        </div>
      </td>
    </tr>
  </table>

  <%@ include file="../buttons.jsp" %>
<br>

  <table class="back-table" border="0" cellspacing="0" cellpadding="1"
         width="100%">
    <tr>
      <td>

        <controls:table tableStyle="front-table" lineStyle="line-row">

          <controls:row header="true"
                labelStyle="table-header-text" dataStyle="table-header-text">
            <controls:label>
              <bean:message key="users.role.properties"/>
            </controls:label>
            <controls:data>
              &nbsp;
            </controls:data>
          </controls:row>

          <controls:row labelStyle="table-label-text"
                         dataStyle="table-normal-text" styleId="rolename">
            <controls:label>
              <bean:message key="users.prompt.rolename"/>
            </controls:label>
            <controls:data>
              <logic:present name="roleForm" property="objectName">
                <bean:write name="roleForm" property="rolename"/>
                <html:hidden property="rolename"/>
              </logic:present>
              <logic:notPresent name="roleForm" property="objectName">
                <html:text property="rolename" size="24" maxlength="32" styleId="rolename"/>
              </logic:notPresent>
            </controls:data>
          </controls:row>

          <controls:row labelStyle="table-label-text"
                         dataStyle="table-normal-text" styleId="description">
            <controls:label>
              <bean:message key="users.prompt.description"/>:
            </controls:label>
            <controls:data>
              <html:text property="description" size="24" maxlength="128" styleId="description"/>
            </controls:data>
          </controls:row>

        </controls:table>

      </td>

    </tr>

  </table>

  <%@ include file="../buttons.jsp" %>

</html:form>

<!-- Standard Footer -->

<%@ include file="footer.jsp" %>

</body>

</html:html>
