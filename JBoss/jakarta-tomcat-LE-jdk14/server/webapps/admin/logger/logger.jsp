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

<html:form method="POST" action="/SaveLogger">

  <bean:define id="thisObjectName" type="java.lang.String"
               name="loggerForm" property="objectName"/>
  <html:hidden property="parentObjectName"/>
  <html:hidden property="adminAction"/>
  <html:hidden property="objectName"/>
  <html:hidden property="loggerType"/>

  <table width="100%" border="0" cellspacing="0" cellpadding="0">
    <tr bgcolor="7171A5">
      <td width="81%">
       <div class="page-title-text" align="left">
         <logic:equal name="loggerForm" property="adminAction" value="Create">
            <bean:message key="actions.loggers.create"/>
          </logic:equal>
          <logic:equal name="loggerForm" property="adminAction" value="Edit">
            <bean:write name="loggerForm" property="nodeLabel"/>
          </logic:equal>
       </div>
      </td>
      <td align="right" nowrap>
        <div class="page-title-text">
      <controls:actions label="Loger Actions">
            <controls:action selected="true"> ----<bean:message key="actions.available.actions"/>---- </controls:action>
            <controls:action disabled="true"> --------------------------------- </controls:action>
            <logic:notEqual name="loggerForm" property="adminAction" value="Create">
            <controls:action url='<%= "/DeleteLogger.do?select=" +
                                  URLEncoder.encode(thisObjectName) %>'>
                <bean:message key="actions.loggers.delete"/>
            </controls:action>
            </logic:notEqual>
       </controls:actions>
         </div>
      </td>
    </tr>
  </table>
    <%@ include file="../buttons.jsp" %>
  <br>

  <table class="back-table" border="0" cellspacing="0" cellpadding="0" width="100%">
    <tr>
      <td>
       <controls:table tableStyle="front-table" lineStyle="line-row">
            <controls:row header="true"
                labelStyle="table-header-text" dataStyle="table-header-text">
            <controls:label><bean:message key="service.property"/></controls:label>
            <controls:data><bean:message key="service.value"/></controls:data>
        </controls:row>

      <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="type">
            <controls:label><bean:message key="connector.type"/>:</controls:label>
            <controls:data>
                 <logic:equal name="loggerForm" property="adminAction" value="Create">
                    <html:select property="loggerType" onchange="IA_jumpMenu('self',this)" styleId="type">
                     <bean:define id="loggerTypeVals" name="loggerForm" property="loggerTypeVals"/>
                     <html:options collection="loggerTypeVals" property="value" labelProperty="label"/>
                    </html:select>
                </logic:equal>
                <logic:equal name="loggerForm" property="adminAction" value="Edit">
                  <bean:write name="loggerForm" property="loggerType" scope="session"/>
                </logic:equal>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="debuglevel">
            <controls:label><bean:message key="server.debuglevel"/>:</controls:label>
            <controls:data>
               <html:select property="debugLvl" styleId="debuglevel">
                     <bean:define id="debugLvlVals" name="loggerForm" property="debugLvlVals"/>
                     <html:options collection="debugLvlVals" property="value"
                        labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="verbositylevel">
            <controls:label><bean:message key="logger.verbositylevel"/>:</controls:label>
            <controls:data>
               <html:select property="verbosityLvl" styleId="verbositylevel">
                     <bean:define id="verbosityLvlVals" name="loggerForm" property="verbosityLvlVals"/>
                     <html:options collection="verbosityLvlVals" property="value"
                        labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>
      </controls:table>
      </td>
    </tr>
  </table>

    <%-- Display the following fields only if it is a FileLogger --%>
    <%-- These are the properties specific to a FileLogger --%>
     <logic:equal name="loggerForm" property="loggerType" scope="session"
                  value="FileLogger">
     <br>

     <table border="0" cellspacing="0" cellpadding="0" width="100%">
        <tr> <td>  <div class="table-title-text">
            <bean:message key="logger.filelogger.properties"/>
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

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="directory">
            <controls:label><bean:message key="logger.directory"/>:</controls:label>
            <controls:data>
               <html:text property="directory" size="25" styleId="directory"/>
               
               
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="prefix">
            <controls:label><bean:message key="logger.prefix"/>:</controls:label>
            <controls:data>
               <html:text property="prefix" size="25" styleId="prefix"/>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="suffix">
            <controls:label><bean:message key="logger.suffix"/>:</controls:label>
            <controls:data>
               <html:text property="suffix" size="15" styleId="suffix"/>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="timestamp">
            <controls:label><bean:message key="logger.timestamp"/>:</controls:label>
            <controls:data>
                <html:select property="timestamp" styleId="timestamp">
                     <bean:define id="booleanVals" name="loggerForm" property="booleanVals"/>
                     <html:options collection="booleanVals" property="value"
                   labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>
   </controls:table>
   </td>
  </tr>
  </table>
 </logic:equal>


    <%@ include file="../buttons.jsp" %>
  <br>
  </html:form>
<p>&nbsp;</p>
</body>
</html:html>
