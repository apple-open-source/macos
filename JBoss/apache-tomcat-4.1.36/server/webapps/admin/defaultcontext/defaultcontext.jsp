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

<html:form method="POST" action="/SaveDefaultContext">
  <bean:define id="thisObjectName" type="java.lang.String"
               name="defaultContextForm" property="objectName"/>
  <html:hidden property="adminAction"/>
  <html:hidden property="objectName"/>
  <html:hidden property="parentObjectName"/>
  <html:hidden property="loaderObjectName"/>
  <html:hidden property="managerObjectName"/>

  <table width="100%" border="0" cellspacing="0" cellpadding="0">
    <tr bgcolor="7171A5">
      <td width="81%">
       <div class="page-title-text" align="left">
          <logic:equal name="defaultContextForm" property="adminAction" value="Create">
            <bean:message key="actions.defaultcontexts.create"/>
          </logic:equal>
          <logic:equal name="defaultContextForm" property="adminAction" value="Edit">
            <bean:write name="defaultContextForm" property="nodeLabel"/>
          </logic:equal>
       </div>
      </td>
      <td align="right" nowrap>
        <div class="page-title-text">
      <controls:actions label="Default Context Actions">
            <controls:action selected="true"> ----<bean:message key="actions.available.actions"/>---- </controls:action>
            <controls:action disabled="true"> --------------------------------- </controls:action>
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
            <bean:message key="defaultcontext.properties"/>
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
                     <bean:define id="booleanVals" name="defaultContextForm" property="booleanVals"/>
                     <html:options collection="booleanVals" property="value"
                   labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="crosscontext">
            <controls:label><bean:message key="context.cross.context"/>:</controls:label>
            <controls:data>
                <html:select property="crossContext" styleId="crosscontext">
                     <bean:define id="booleanVals" name="defaultContextForm" property="booleanVals"/>
                     <html:options collection="booleanVals" property="value"
                   labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

      <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="reloadable">
            <controls:label><bean:message key="context.reloadable"/>:</controls:label>
            <controls:data>
                <html:select property="reloadable" styleId="reloadable">
                     <bean:define id="booleanVals" name="defaultContextForm" property="booleanVals"/>
                     <html:options collection="booleanVals" property="value"
                   labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

      <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="swallowoutput">
            <controls:label><bean:message key="context.swallowOutput"/>:</controls:label>
            <controls:data>
                <html:select property="swallowOutput" styleId="swallowoutput">
                     <bean:define id="booleanVals" name="defaultContextForm" property="booleanVals"/>
                     <html:options collection="booleanVals" property="value"
                   labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

      <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="usenaming">
            <controls:label><bean:message key="context.usenaming"/>:</controls:label>
            <controls:data>
                <html:select property="useNaming" styleId="usenaming">
                     <bean:define id="booleanVals" name="defaultContextForm" property="booleanVals"/>
                     <html:options collection="booleanVals" property="value"
                   labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>
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

      <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="debuglevel">
            <controls:label><bean:message key="server.debuglevel"/>:</controls:label>
            <controls:data>
               <html:select property="ldrDebugLvl" styleId="debuglevel">
                     <bean:define id="debugLvlVals" name="defaultContextForm" property="debugLvlVals"/>
                     <html:options collection="debugLvlVals" property="value"
                        labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

      <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="reloadable">
            <controls:label><bean:message key="context.reloadable"/>:</controls:label>
            <controls:data>
                <html:select property="ldrReloadable" styleId="reloadable">
                     <bean:define id="booleanVals" name="defaultContextForm" property="booleanVals"/>
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

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="checkinterval">
            <controls:label><bean:message key="context.checkInterval"/>:</controls:label>
            <controls:data>
                <html:text property="mgrCheckInterval" size="5" styleId="checkinterval"/>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="debuglevel">
            <controls:label><bean:message key="server.debuglevel"/>:</controls:label>
            <controls:data>
               <html:select property="mgrDebugLvl" styleId="debuglevel">
                     <bean:define id="debugLvlVals" name="defaultContextForm" property="debugLvlVals"/>
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

       <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="maxsessions">
            <controls:label><bean:message key="context.max.sessions"/>:</controls:label>
            <controls:data>
               <html:text property="mgrMaxSessions" size="5" styleId="maxsessions"/>
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
