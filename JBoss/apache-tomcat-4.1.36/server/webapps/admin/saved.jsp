<!-- Standard Struts Entries -->

<%@ page language="java" contentType="text/html;charset=utf-8" %>
<%@ taglib uri="/WEB-INF/struts-bean.tld" prefix="bean" %>
<%@ taglib uri="/WEB-INF/struts-html.tld" prefix="html" %>
<%@ taglib uri="/WEB-INF/struts-logic.tld" prefix="logic" %>

<html:html locale="true">

  <body bgcolor="white" background="images/PaperTexture.gif">

    <%-- Cause our tree control to refresh itself --%>
    <script language="JavaScript">
      <!--
        parent.tree.location='treeControlTest.do';
      -->
    </script>

    <%@ include file="header.jsp" %>

    <%-- display warnings if any --%>
    <logic:present name="warning">
            <bean:message key="warning.header"/>
            <bean:message key='<%= (String) request.getAttribute("warning") %>'/>
            <br>
    </logic:present>

    <center><h2>
      <bean:message key="save.success"/>
    </h2></center>

    <%@ include file="footer.jsp" %>

  </body>

</html:html>
