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

<html:form method="post" action="/DeleteValves">

  <bean:define id="thisParentName" type="java.lang.String"
               name="valvesForm" property="parentObjectName"/>
  <html:hidden property="parentObjectName"/>

  <table width="100%" border="0" cellspacing="0" cellpadding="0">
    <tr bgcolor="7171A5">
      <td width="81%">
        <div class="page-title-text" align="left">
          <bean:message key="actions.valves.delete"/>
        </div>
      </td>
      <td align="right" nowrap>
        <div class="page-title-text">
            <controls:actions label="Valve Actions">
              <controls:action selected="true">
                ----<bean:message key="actions.available.actions"/>----
              </controls:action>
              <controls:action disabled="true">
                ---------------------------------
              </controls:action>
            </controls:actions>
        </div>
      </td>
    </tr>
  </table>

<%@ include file="../buttons.jsp" %>
  <br>

  <%-- Valves List --%>

  <table class="back-table" border="0" cellspacing="0" cellpadding="1"
         width="100%">
    <tr><td>

      <table class="front-table" border="1"
       cellspacing="0" cellpadding="0" width="100%">

        <tr class="header-row">
          <td><div align="left" class="table-header-text">
            <bean:message key="actions.delete"/>
          </div></td>
          <td><div align="left" class="table-header-text">
            <bean:message key="host.name"/>
          </div></td>
        </tr>

        <logic:iterate name="valvesList" id="valve">
          <tr class="line-row">
            <td><div align="left" class="table-normal-text">&nbsp;
            <label for="valves"></label>
              <html:multibox property="valves"
                                value="<%= valve.toString() %>" styleId="valves"/>
            </div></td>
            <td><div align="left" class="table-normal-text">&nbsp;
              <html:link page='<%= "/EditValve.do?select=" +
                         java.net.URLEncoder.encode(valve.toString()) +
                         "&parent="+ URLEncoder.encode(thisParentName) %>'>
                <controls:attribute name="valve" attribute="className"/>
              </html:link>
            </div></td>
          </tr>
        </logic:iterate>

      </table>

    </td></tr>
  </table>

<%@ include file="../buttons.jsp" %>

  <br>
</html:form>

<p>&nbsp;</p>
</body>
</html:html>
