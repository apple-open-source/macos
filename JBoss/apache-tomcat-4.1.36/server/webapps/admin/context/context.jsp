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

<html:form method="POST" action="/SaveContext">
  <bean:define id="thisObjectName" type="java.lang.String"
               name="contextForm" property="objectName"/>
  <html:hidden property="adminAction"/>
  <html:hidden property="objectName"/>
  <html:hidden property="parentObjectName"/>
  <html:hidden property="loaderObjectName"/>
  <html:hidden property="managerObjectName"/>

  <table width="100%" border="0" cellspacing="0" cellpadding="0">
    <tr bgcolor="7171A5">
      <td width="81%">
       <div class="page-title-text" align="left">
          <logic:equal name="contextForm" property="adminAction" value="Create">
            <bean:message key="actions.contexts.create"/>
          </logic:equal>
          <logic:equal name="contextForm" property="adminAction" value="Edit">
            <bean:write name="contextForm" property="nodeLabel"/>
          </logic:equal>
       </div>
      </td>
      <td align="right" nowrap>
        <div class="page-title-text">
      <controls:actions label="Context Actions">
            <controls:action selected="true"> ----<bean:message key="actions.available.actions"/>---- </controls:action>
            <controls:action disabled="true"> --------------------------------- </controls:action>
            <logic:notEqual name="contextForm" property="adminAction" value="Create">
            <controls:action url='<%= "/AddLogger.do?parent=" +
                                  URLEncoder.encode(thisObjectName) %>'>
                <bean:message key="actions.loggers.create"/>
            </controls:action>
            <controls:action url='<%= "/DeleteLogger.do?parent=" +
                                  URLEncoder.encode(thisObjectName) %>'>
                <bean:message key="actions.loggers.deletes"/>
            </controls:action>
            <%-- cannot delete or add the realm of the context of the admin app --%>
            <logic:notEqual name="contextForm" property="path"
                            value='<%= request.getContextPath() %>'>
            <controls:action disabled="true"> ------------------------------------- </controls:action>
            <controls:action url='<%= "/AddRealm.do?parent=" +
                                  URLEncoder.encode(thisObjectName) %>'>
                <bean:message key="actions.realms.create"/>
            </controls:action>
            <controls:action url='<%= "/DeleteRealm.do?parent=" +
                                  URLEncoder.encode(thisObjectName) %>'>
                <bean:message key="actions.realms.deletes"/>
            </controls:action>
            </logic:notEqual>
            <controls:action disabled="true">  -------------------------------------  </controls:action>
            <controls:action url='<%= "/AddValve.do?parent=" +
                                  URLEncoder.encode(thisObjectName) %>'>
               <bean:message key="actions.valves.create"/>
            </controls:action>
            <controls:action url='<%= "/DeleteValve.do?parent=" +
                                  URLEncoder.encode(thisObjectName) %>'>
               <bean:message key="actions.valves.deletes"/>
            </controls:action>
            <%-- cannot delete the context of the admin app  from the tool --%>
            <logic:notEqual name="contextForm" property="path"
                            value='<%= request.getContextPath() %>'>
            <controls:action disabled="true">  -------------------------------------  </controls:action>
            <controls:action url='<%= "/DeleteContext.do?select=" +
                                        URLEncoder.encode(thisObjectName) %>'>
                <bean:message key="actions.contexts.delete"/>
            </controls:action>
            </logic:notEqual>
            </logic:notEqual>
        </controls:actions>
         </div>
      </td>
    </tr>
  </table>
    <%@ include file="../buttons.jsp" %>
  <br>

 <%-- Context Properties table --%>

 <table border="0" cellspacing="0" cellpadding="0" width="100%">
    <tr> <td>  <div class="table-title-text">
            <bean:message key="context.properties"/>
    </div> </td> </tr>
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

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="cookies">
            <controls:label><bean:message key="context.cookies"/>:</controls:label>
            <controls:data>
                <html:select property="cookies" styleId="cookies">
                     <bean:define id="booleanVals" name="contextForm" property="booleanVals"/>
                     <html:options collection="booleanVals" property="value"
                   labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="crossContext">
            <controls:label><bean:message key="context.cross.context"/>:</controls:label>
            <controls:data>
                <html:select property="crossContext" styleId="crossContext">
                     <bean:define id="booleanVals" name="contextForm" property="booleanVals"/>
                     <html:options collection="booleanVals" property="value"
                   labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="debuglevel">
            <controls:label><bean:message key="server.debuglevel"/>:</controls:label>
            <controls:data>
               <html:select property="debugLvl" styleId="debuglevel">
                     <bean:define id="debugLvlVals" name="contextForm" property="debugLvlVals"/>
                     <html:options collection="debugLvlVals" property="value"
                        labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

<%-- input only allowed on create transaction --%>
       <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="docbase">
            <controls:label><bean:message key="context.docBase"/>:</controls:label>
            <controls:data>
              <logic:equal name="contextForm" property="adminAction" value="Create">
               <html:text property="docBase" size="30" styleId="docbase"/>
              </logic:equal>
              <logic:equal name="contextForm" property="adminAction" value="Edit">
               <bean:write name="contextForm" property="docBase"/>
               <html:hidden property="docBase"/>
              </logic:equal>
            </controls:data>
        </controls:row>

      <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="override">
            <controls:label><bean:message key="context.override"/>:</controls:label>
            <controls:data>
                <html:select property="override" styleId="override">
                     <bean:define id="booleanVals" name="contextForm" property="booleanVals"/>
                     <html:options collection="booleanVals" property="value"
                   labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

      <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="privileged">
            <controls:label><bean:message key="context.privileged"/>:</controls:label>
            <controls:data>
                <html:select property="privileged" styleId="privileged">
                     <bean:define id="booleanVals" name="contextForm" property="booleanVals"/>
                     <html:options collection="booleanVals" property="value"
                   labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

<%-- input only allowed on create transaction --%>
       <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="path">
            <controls:label><bean:message key="context.path"/>:</controls:label>
            <controls:data>
             <logic:equal name="contextForm" property="adminAction" value="Create">
               <html:text property="path" size="30" styleId="path"/>
             </logic:equal>
             <logic:equal name="contextForm" property="adminAction" value="Edit">
               <bean:write name="contextForm" property="path"/>
               <html:hidden property="path"/>
             </logic:equal>
            </controls:data>
        </controls:row>

      <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="reloadable">
            <controls:label><bean:message key="context.reloadable"/>:</controls:label>
            <controls:data>
                <html:select property="reloadable" styleId="reloadable">
                     <bean:define id="booleanVals" name="contextForm" property="booleanVals"/>
                     <html:options collection="booleanVals" property="value"
                   labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

      <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="swallowOutput">
            <controls:label><bean:message key="context.swallowOutput"/>:</controls:label>
            <controls:data>
                <html:select property="swallowOutput" styleId="swallowOutput">
                     <bean:define id="booleanVals" name="contextForm" property="booleanVals"/>
                     <html:options collection="booleanVals" property="value"
                   labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

      <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="usernaming">
            <controls:label><bean:message key="context.usenaming"/>:</controls:label>
            <controls:data>
                <html:select property="useNaming" styleId="usernaming">
                     <bean:define id="booleanVals" name="contextForm" property="booleanVals"/>
                     <html:options collection="booleanVals" property="value"
                   labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

      <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="antiJarLocking">
            <controls:label><bean:message key="context.antiJarLocking"/>:</controls:label>
            <controls:data>
                <html:select property="antiJarLocking" styleId="antiJarLocking">
                     <bean:define id="booleanVals" name="contextForm" property="booleanVals"/>
                     <html:options collection="booleanVals" property="value"
                   labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

<%-- input only allowed on create transaction >
       <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="workdir">
            <controls:label><bean:message key="context.workdir"/>:</controls:label>
            <controls:data>
             <logic:equal name="contextForm" property="adminAction" value="Create">
               <html:text property="workDir" size="30" styleId="workdir"/>
             </logic:equal>
             <logic:equal name="contextForm" property="adminAction" value="Edit">
               <bean:write name="contextForm" property="workDir"/>
               <html:hidden property="workDir"/>
             </logic:equal>
            </controls:data>
        </controls:row--%>
   </controls:table>
    </td>
  </tr>
</table>

<br>

<%-- Loader Properties table --%>

 <table border="0" cellspacing="0" cellpadding="0" width="100%">
    <tr> <td>  <div class="table-title-text">
            <bean:message key="context.loader.properties"/>
    </div> </td> </tr>
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

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="checkInterval">
            <controls:label><bean:message key="context.checkInterval"/>:</controls:label>
            <controls:data>
                <html:text property="ldrCheckInterval" size="5" styleId="checkInterval"/>
            </controls:data>
        </controls:row>

      <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="debugLvlVals">
            <controls:label><bean:message key="server.debuglevel"/>:</controls:label>
            <controls:data>
               <html:select property="ldrDebugLvl" styleId="debugLvlVals">
                     <bean:define id="debugLvlVals" name="contextForm" property="debugLvlVals"/>
                     <html:options collection="debugLvlVals" property="value"
                        labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

      <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="reloadable">
            <controls:label><bean:message key="context.reloadable"/>:</controls:label>
            <controls:data>
                <html:select property="ldrReloadable" styleId="reloadable">
                     <bean:define id="booleanVals" name="contextForm" property="booleanVals"/>
                     <html:options collection="booleanVals" property="value"
                   labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>
   </controls:table>
    </td>
  </tr>
</table>

<BR>
<%-- Session Manager Properties table --%>
 <table border="0" cellspacing="0" cellpadding="0" width="100%">
    <tr> <td>  <div class="table-title-text">
            <bean:message key="context.sessionmgr.properties"/>
    </div> </td> </tr>
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

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="checkInterval">
            <controls:label><bean:message key="context.checkInterval"/>:</controls:label>
            <controls:data>
                <html:text property="mgrCheckInterval" size="5" styleId="checkInterval"/>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="debuglevel">
            <controls:label><bean:message key="server.debuglevel"/>:</controls:label>
            <controls:data>
               <html:select property="mgrDebugLvl" styleId="debuglevel">
                     <bean:define id="debugLvlVals" name="contextForm" property="debugLvlVals"/>
                     <html:options collection="debugLvlVals" property="value"
                        labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

       <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="sessionId">
            <controls:label><bean:message key="context.sessionId"/>:</controls:label>
            <controls:data>
               <html:textarea property="mgrSessionIDInit" cols="30" rows="2" styleId="sessionId"/>
            </controls:data>
        </controls:row>

       <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="maxSessions">
            <controls:label><bean:message key="context.max.sessions"/>:</controls:label>
            <controls:data>
               <html:text property="mgrMaxSessions" size="5" styleId="maxSessions"/>
            </controls:data>
        </controls:row>
   </controls:table>
    </td>
  </tr>
</table>

    <%@ include file="../buttons.jsp" %>

  <br>
  </html:form>
<p>&nbsp;</p>
</body>
</html:html>
