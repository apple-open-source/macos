<!-- Standard Struts Entries -->
<%@ page language="java" import="java.net.URLEncoder" contentType="text/html;charset=utf-8" %>
<%@ taglib uri="/WEB-INF/struts-bean.tld" prefix="bean" %>
<%@ taglib uri="/WEB-INF/struts-html.tld" prefix="html" %>
<%@ taglib uri="/WEB-INF/controls.tld" prefix="controls" %>
<%@ taglib uri="/WEB-INF/struts-logic.tld" prefix="logic" %>

<html:html locale="true">

<%@ include file="../users/header.jsp" %>

<!-- Body -->
<body bgcolor="white" background="../images/PaperTexture.gif">

<!--Form -->

<html:errors/>

<html:form method="POST" action="/SaveJNDIRealm">
  <bean:define id="thisObjectName" type="java.lang.String"
               name="jndiRealmForm" property="objectName"/>
  <html:hidden property="adminAction"/>
  <html:hidden property="objectName"/>
  <html:hidden property="parentObjectName"/>
  <html:hidden property="allowDeletion"/>

  <table width="100%" border="0" cellspacing="0" cellpadding="0">
    <tr bgcolor="7171A5">
      <td width="81%">
       <div class="page-title-text" align="left">
         <logic:equal name="jndiRealmForm" property="adminAction" value="Create">
            <bean:message key="actions.realms.create"/>
          </logic:equal>
          <logic:equal name="jndiRealmForm" property="adminAction" value="Edit">
            <bean:write name="jndiRealmForm" property="nodeLabel"/>
          </logic:equal>
       </div>
      </td>
      <td align="right" nowrap>
        <div class="page-title-text">
      <controls:actions label="Realm Actions">
            <controls:action selected="true"> ----<bean:message key="actions.available.actions"/>---- </controls:action>
            <controls:action> --------------------------------- </controls:action>
            <logic:notEqual name="jndiRealmForm" property="adminAction" value="Create">
                <logic:notEqual name="jndiRealmForm" property="allowDeletion" value="false">
                <controls:action url='<%= "/DeleteRealm.do?select=" +
                                        URLEncoder.encode(thisObjectName) %>'>
                <bean:message key="actions.realms.delete"/>
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
                 <logic:equal name="jndiRealmForm" property="adminAction" value="Create">
                    <html:select property="realmType" onchange="IA_jumpMenu('self',this)" styleId="type">
                     <bean:define id="realmTypeVals" name="jndiRealmForm" property="realmTypeVals"/>
                     <html:options collection="realmTypeVals" property="value" labelProperty="label"/>
                    </html:select>
                </logic:equal>
                <logic:equal name="jndiRealmForm" property="adminAction" value="Edit">
                  <bean:write name="jndiRealmForm" property="realmType" scope="session"/>
                </logic:equal>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="connName">
            <controls:label><bean:message key="realm.connName"/>:</controls:label>
            <controls:data>
              <html:text property="connectionName" size="30" styleId="connName"/>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="connPassword">
            <controls:label><bean:message key="realm.connPassword"/>:</controls:label>
            <controls:data>
                <html:text property="connectionPassword" size="30" styleId="connPassword"/>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="connURL">
            <controls:label><bean:message key="realm.connURL"/>:</controls:label>
            <controls:data>
                <html:text property="connectionURL" size="30" styleId="connURL"/>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="connFactory">
            <controls:label><bean:message key="realm.connFactory"/>:</controls:label>
            <controls:data>
                <html:text property="contextFactory" size="30" styleId="connFactory"/>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="debuglevel">
            <controls:label><bean:message key="server.debuglevel"/>:</controls:label>
            <controls:data>
               <html:select property="debugLvl" styleId="debuglevel">
                     <bean:define id="debugLvlVals" name="jndiRealmForm" property="debugLvlVals"/>
                     <html:options collection="debugLvlVals" property="value"
                        labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

       <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="digest">
            <controls:label><bean:message key="realm.digest"/>:</controls:label>
            <controls:data>
                <html:text property="digest" size="30" styleId="digest"/>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="roleBase">
            <controls:label><bean:message key="realm.roleBase"/>:</controls:label>
            <controls:data>
                <html:text property="roleBase" size="30" styleId="roleBase"/>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="roleName">
            <controls:label><bean:message key="realm.roleName"/>:</controls:label>
            <controls:data>
                <html:text property="roleName" size="30" styleId="roleName"/>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="pattern">
            <controls:label><bean:message key="realm.pattern"/>:</controls:label>
            <controls:data>
                <html:text property="rolePattern" size="30" styleId="pattern"/>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="rolesubtree">
            <controls:label><bean:message key="realm.role.subtree"/>:</controls:label>
            <controls:data>
             <html:select property="roleSubtree" styleId="rolesubtree">
                     <bean:define id="searchVals" name="jndiRealmForm" property="searchVals"/>
                     <html:options collection="searchVals" property="value"
                        labelProperty="label"/>
                </html:select>
              </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="userBase">
            <controls:label><bean:message key="realm.userBase"/>:</controls:label>
            <controls:data>
                <html:text property="userBase" size="30" styleId="userBase"/>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="roleName">
            <controls:label><bean:message key="realm.user.roleName"/>:</controls:label>
            <controls:data>
                <html:text property="userRoleName" size="30" styleId="roleName"/>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="usersubtree">
            <controls:label><bean:message key="realm.user.subtree"/>:</controls:label>
            <controls:data>
             <html:select property="userSubtree" styleId="usersubtree">
                     <bean:define id="searchVals" name="jndiRealmForm" property="searchVals"/>
                     <html:options collection="searchVals" property="value"
                        labelProperty="label"/>
                </html:select>
              </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="userPassword">
            <controls:label><bean:message key="realm.userPassword"/>:</controls:label>
            <controls:data>
                <html:text property="userPassword" size="30" styleId="userPassword"/>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="userPattern">
            <controls:label><bean:message key="realm.userPattern"/>:</controls:label>
            <controls:data>
                <html:text property="userPattern" size="30" styleId="userPattern"/>
            </controls:data>
        </controls:row>

       <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="userSearch">
           <controls:label><bean:message key="realm.userSearch"/>:</controls:label>
           <controls:data>
               <html:text property="userSearch" size="30" styleId="userSearch"/>
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
