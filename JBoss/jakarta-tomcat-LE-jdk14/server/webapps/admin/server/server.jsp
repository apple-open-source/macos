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

<html:form method="POST" action="/SaveServer" focus="portNumberText">
  <table width="100%" border="0" cellspacing="0" cellpadding="0">
    <tr class="page-title-row">
      <td align="left" nowrap>
        <div class="page-title-text">
           <bean:write name="serverForm" property="nodeLabel"/>
        </div>
      </td>
      <td align="right" nowrap>
        <div class="page-title-text">
        <controls:actions label="Server Actions">
          <controls:action selected="true">
            ----<bean:message key="actions.available.actions"/>----
          </controls:action>
          <controls:action>
            ---------------------------------
          </controls:action>
          <controls:action url="/AddService.do">
            <bean:message key="actions.services.create"/>
          </controls:action>
          <controls:action url="/DeleteService.do">
            <bean:message key="actions.services.deletes"/>
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
      <bean:message key="server.properties"/>
    </div></td></tr>
  </table>

  <table class="back-table" border="0" cellspacing="0" cellpadding="1" width="100%">
    <tr>
      <td>
         <controls:table tableStyle="front-table" lineStyle="line-row">
          <controls:row header="true"
              labelStyle="table-header-text" dataStyle="table-header-text">
            <controls:label>
              <bean:message key="service.property"/>
            </controls:label>
            <controls:data>
              <bean:message key="service.value"/>
            </controls:data>
          </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="portNumber">
            <controls:label><bean:message key="server.portnumber"/>:</controls:label>
            <controls:data>
              <html:text property="portNumberText" size="24" maxlength="24" styleId="portNumber"/>
            </controls:data>
        </controls:row>

        <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="debugLvl">
            <controls:label><bean:message key="server.debuglevel"/>:</controls:label>
            <controls:data>
                <html:select property="debugLvl" styleId="debugLvl">
                     <bean:define id="debugLvlVals" name="serverForm" property="debugLvlVals"/>
                     <html:options collection="debugLvlVals" property="value"
                      labelProperty="label"/>
                </html:select>
            </controls:data>
        </controls:row>

       <controls:row labelStyle="table-label-text" dataStyle="table-normal-text" styleId="shutdown">
            <controls:label><bean:message key="server.shutdown"/>:</controls:label>
            <controls:data>
               <html:text property="shutdownText" size="24" maxlength="24" styleId="shutdown"/>
            </controls:data>
        </controls:row>
      </controls:table>

      </td>
    </tr>
  </table>

  <%@ include file="../buttons.jsp" %>

</html:form>

<!-- Standard Footer -->

<%@ include file="../footer.jsp" %>

</body>

</html:html>
