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

<html:form method="POST" action="/resources/saveEnvEntry">

  <html:hidden property="objectName"/>

  <bean:define id="resourcetypeInfo" type="java.lang.String"
               name="envEntryForm" property="resourcetype"/>
  <html:hidden property="resourcetype"/>

  <bean:define id="pathInfo" type="java.lang.String"
               name="envEntryForm" property="path"/>
  <html:hidden property="path"/>

  <bean:define id="hostInfo" type="java.lang.String"
               name="envEntryForm" property="host"/>
  <html:hidden property="host"/>

  <bean:define id="serviceInfo" type="java.lang.String"
               name="envEntryForm" property="service"/>
  <html:hidden property="service"/>

  <table width="100%" border="0" cellspacing="0" cellpadding="0">
    <tr class="page-title-row">
      <td align="left" nowrap>
        <div class="page-title-text">
          <bean:write name="envEntryForm" property="nodeLabel"/>
        </div>
      </td>
      <td align="right" nowrap>
        <div class="page-title-text">
          <controls:actions label="Environment Entry Actions">
            <controls:action selected="true">
              ----<bean:message key="actions.available.actions"/>----
            </controls:action>
            <controls:action>
              ---------------------------------
            </controls:action>

        <controls:action url='<%= "/resources/setUpEnvEntry.do?resourcetype=" +
                            URLEncoder.encode(resourcetypeInfo) + "&path="+
                            URLEncoder.encode(pathInfo) + "&host="+
                            URLEncoder.encode(hostInfo) + "&service="+
                            URLEncoder.encode(serviceInfo) %>'>
                <bean:message key="resources.actions.env.create"/>
            </controls:action>
            <controls:action url='<%= "/resources/listEnvEntries.do?resourcetype=" +
                            URLEncoder.encode(resourcetypeInfo) + "&path="+
                            URLEncoder.encode(pathInfo) + "&host="+
                            URLEncoder.encode(hostInfo) + "&service="+
                            URLEncoder.encode(serviceInfo) + "&forward=" +
                            URLEncoder.encode("EnvEntries Delete List") %>'>
                <bean:message key="resources.actions.env.delete"/>
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
      <bean:message key="resources.env.props"/>
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
              <logic:present name="envEntryForm" property="objectName">
                <bean:write name="envEntryForm" property="name"/>
                <html:hidden property="name"/>
              </logic:present>
              <logic:notPresent name="envEntryForm" property="objectName">
                <html:text property="name" size="24" maxlength="32" styleId="name"/>
              </logic:notPresent>
            </controls:data>
          </controls:row>

          <controls:row labelStyle="table-label-text"
                         dataStyle="table-normal-text" styleId="type">
            <controls:label>
              <bean:message key="connector.type"/>:
            </controls:label>
            <controls:data>
              <html:select property="entryType" styleId="type">
                     <bean:define id="typeVals" name="envEntryForm" property="typeVals"/>
                     <html:options collection="typeVals" property="value"
                                   labelProperty="label"/>
                </html:select>
            </controls:data>
          </controls:row>

          <controls:row labelStyle="table-label-text"
                         dataStyle="table-normal-text" styleId="value">
            <controls:label>
              <bean:message key="service.value"/>:
            </controls:label>
            <controls:data>
              <html:text property="value" size="24" maxlength="64" styleId="value"/>
            </controls:data>
          </controls:row>

          <controls:row labelStyle="table-label-text"
                         dataStyle="table-normal-text" styleId="override">
            <controls:label>
              <bean:message key="resources.env.override"/>:
            </controls:label>
            <controls:data>
              <html:checkbox property="override" value="override" styleId="override"/>
            </controls:data>
          </controls:row>

          <controls:row labelStyle="table-label-text"
                         dataStyle="table-normal-text" styleId="description">
            <controls:label>
              <bean:message key="users.prompt.description"/>
            </controls:label>
            <controls:data>
              <html:text property="description" size="30" maxlength="64" styleId="description"/>
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
