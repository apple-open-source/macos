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

<html:form method="POST" action="/SaveConnector">

  <bean:define id="thisObjectName" type="java.lang.String"
               name="connectorForm" property="objectName"/>
  <html:hidden property="connectorName"/>
  <html:hidden property="adminAction"/>
  <html:hidden property="objectName"/>
  <html:hidden property="connectorType"/>
  <html:hidden property="serviceName"/>

  <table width="100%" border="0" cellspacing="0" cellpadding="0">
    <tr bgcolor="7171A5">
      <td width="81%">
       <div class="page-title-text" align="left">
          <logic:equal name="connectorForm" property="adminAction" value="Create">
            <bean:message key="actions.connectors.create"/>
          </logic:equal>
          <logic:equal name="connectorForm" property="adminAction" value="Edit">
           <bean:write name="connectorForm" property="nodeLabel"/>
          </logic:equal>
       </div>
      </td>
      <td align="right" nowrap>
        <div class="page-title-text">
      <controls:actions label="Connector Actions">
            <controls:action selected="true"> ----<bean:message key="actions.available.actions"/>---- </controls:action>
            <controls:action> --------------------------------- </controls:action>
            <logic:notEqual name="connectorForm" property="adminAction" value="Create">
            <logic:notEqual name="connectorForm" property="portText"
                            value='<%= Integer.toString(request.getServerPort()) %>'>
            <controls:action url='<%= "/DeleteConnector.do?select=" +
                                        URLEncoder.encode(thisObjectName) %>'>
            <bean:message key="actions.connectors.delete"/>
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

  <table class="back-table" border="0" cellspacing="0" cellpadding="1" width="100%">
    <tr>
      <td>
       <controls:table tableStyle="front-table" lineStyle="line-row">
            <controls:row header="true"
                labelStyle="table-header-text" dataStyle="table-header-text">
            <controls:label>General</controls:label>
            <controls:data>&nbsp;</controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="connectorType">
            <controls:label><bean:message key="connector.type"/>:</controls:label>
            <controls:data>
                 <logic:equal name="connectorForm" property="adminAction" value="Create">
                    <html:select property="connectorType" onchange="IA_jumpMenu('self',this)" styleId="connectorType">
                     <bean:define id="connectorTypeVals" name="connectorForm" property="connectorTypeVals"/>
                     <html:options collection="connectorTypeVals" property="value" labelProperty="label"/>
                    </html:select>
                </logic:equal>
                <logic:equal name="connectorForm" property="adminAction" value="Edit">
                  <bean:write name="connectorForm" property="connectorType" scope="session"/>
                </logic:equal>
            </controls:data>
        </controls:row>

    <%-- do not show scheme while creating a new connector --%>
    <logic:notEqual name="connectorForm" property="adminAction" value="Create">
        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text">
            <controls:label><bean:message key="connector.scheme"/>:</controls:label>
            <controls:data>
              <bean:write name="connectorForm" property="scheme" scope="session"/>
            </controls:data>
        </controls:row>
     </logic:notEqual>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="acceptCount">
            <controls:label><bean:message key="connector.accept.count"/>:</controls:label>
            <controls:data>
              <html:text property="acceptCountText" size="5" maxlength="5" styleId="acceptCount"/>
            </controls:data>
        </controls:row>

       <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="timeout">
            <controls:label><bean:message key="connector.connection.timeout"/><br>
                (<bean:message key="connector.milliseconds"/>) :</controls:label>
            <controls:data>
               <html:text property="connTimeOutText" size="10" styleId="timeout"/>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="debuglevel">
            <controls:label><bean:message key="server.debuglevel"/>:</controls:label>
            <controls:data>
               <html:select property="debugLvl" styleId="debuglevel">
                     <bean:define id="debugLvlVals" name="connectorForm" property="debugLvlVals"/>
                     <html:options collection="debugLvlVals" property="value"
                        labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="buffersize">
            <controls:label><bean:message key="connector.default.buffer"/>:</controls:label>
            <controls:data>
               <html:text property="bufferSizeText" size="5" styleId="buffersize"/>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="enableDNS">
            <controls:label><bean:message key="connector.enable.dns"/>:</controls:label>
            <controls:data>
                <html:select property="enableLookups" styleId="enableDNS">
                     <bean:define id="booleanVals" name="connectorForm" property="booleanVals"/>
                     <html:options collection="booleanVals" property="value"
                   labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

        <%-- Input only allowed on create transaction --%>
        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="address">
            <controls:label><bean:message key="connector.address.ip"/>:</controls:label>
            <controls:data>
             <logic:equal name="connectorForm" property="adminAction" value="Create">
               <html:text property="address" size="20" styleId="address"/>
             </logic:equal>
             <logic:equal name="connectorForm" property="adminAction" value="Edit">
               &nbsp;<bean:write name="connectorForm" property="address"/>
               <html:hidden property="address"/>
             </logic:equal>
            </controls:data>
        </controls:row>

        <controls:row header="true" labelStyle="table-header-text" dataStyle="table-header-text">
            <controls:label>Ports</controls:label>
            <controls:data>&nbsp;</controls:data>
        </controls:row>

        <%-- Input only allowed on create transaction --%>
        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="portnumber">
            <controls:label><bean:message key="server.portnumber"/>:</controls:label>
            <controls:data>
             <logic:equal name="connectorForm" property="adminAction" value="Create">
               <html:text property="portText" size="5" styleId="portnumer"/>
             </logic:equal>
             <logic:equal name="connectorForm" property="adminAction" value="Edit">
               <bean:write name="connectorForm" property="portText"/>
               <html:hidden property="portText"/>
             </logic:equal>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="redirectport">
            <controls:label><bean:message key="connector.redirect.portnumber"/>:</controls:label>
            <controls:data>
               <html:text property="redirectPortText" size="5" styleId="redirectport"/>
            </controls:data>
        </controls:row>

        <controls:row header="true" labelStyle="table-header-text" dataStyle="table-header-text">
            <controls:label>Processors</controls:label>
            <controls:data>&nbsp;</controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="minProcessor">
            <controls:label><bean:message key="connector.min"/>:</controls:label>
            <controls:data>
               <html:text property="minProcessorsText" size="5" styleId="minProcessor"/>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="connectorMax">
            <controls:label><bean:message key="connector.max"/>:</controls:label>
            <controls:data>
               <html:text property="maxProcessorsText" size="5" styleId="connectorMax"/>
            </controls:data>
        </controls:row>

<%-- The following properties are supported only for Coyote HTTP/S 1.1 Connectors --%>
     <logic:notEqual name="connectorForm" property="connectorType" scope="session"
                  value="AJP">
        <controls:row header="true" labelStyle="table-header-text" dataStyle="table-header-text">
            <controls:label>Proxy</controls:label>
            <controls:data>&nbsp;</controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="proxyName">
            <controls:label><bean:message key="connector.proxy.name"/>:</controls:label>
            <controls:data>
               <html:text property="proxyName" size="30" styleId="proxyName"/>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="portNumber">
            <controls:label><bean:message key="connector.proxy.portnumber"/>:</controls:label>
            <controls:data>
                <html:text property="proxyPortText" size="5" styleId="portNumber"/>
            </controls:data>
        </controls:row>
        </logic:notEqual>

<%-- The following properties are supported only on HTTPS Connector --%>
     <logic:equal name="connectorForm" property="scheme" scope="session"
                  value="https">
        <br>
        <controls:row header="true" labelStyle="table-header-text" dataStyle="table-header-text">
            <controls:label>Factory Properties:</controls:label>
            <controls:data>&nbsp;</controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="clientauth">
            <controls:label><bean:message key="connector.client.auth"/>:</controls:label>
            <controls:data>
                <html:select property="clientAuthentication" styleId="clientauth">
                     <bean:define id="booleanVals" name="connectorForm" property="booleanVals"/>
                     <html:options collection="booleanVals" property="value"
                   labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

        <%-- Input allowed only on create --%>
        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="keystore">
            <controls:label><bean:message key="connector.keystore.filename"/>:</controls:label>
            <controls:data>
            <logic:equal name="connectorForm" property="adminAction" value="Create">
                <html:text property="keyStoreFileName" size="30" styleId="keystore"/>
             </logic:equal>
             <logic:equal name="connectorForm" property="adminAction" value="Edit">
               <bean:write name="connectorForm" property="keyStoreFileName"/>
             </logic:equal>
            </controls:data>
        </controls:row>

        <%-- input password allowed only while creating connector --%>
        <logic:equal name="connectorForm" property="adminAction" value="Create">
        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="password">
            <controls:label><bean:message key="connector.keystore.password"/>:</controls:label>
            <controls:data>
                <html:password property="keyStorePassword" size="30" styleId="password"/>
                <%--
                <logic:equal name="connectorForm" property="adminAction" value="Edit">
                   <bean:write name="connectorForm" property="keyStorePassword"/>
                </logic:equal>
                --%>
            </controls:data>
        </controls:row>
        </logic:equal>

    </logic:equal>
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
