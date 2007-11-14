<!-- Standard Struts Entries -->

<%@ page language="java" contentType="text/html;charset=utf-8" %>
<%@ taglib uri="/WEB-INF/struts-bean.tld" prefix="bean" %>
<%@ taglib uri="/WEB-INF/struts-html.tld" prefix="html" %>
<%@ taglib uri="/WEB-INF/struts-logic.tld" prefix="logic" %>
<%@ taglib uri="/WEB-INF/controls.tld" prefix="controls" %>

<html:html locale="true">

<!-- Standard Content -->

<%@ include file="header.jsp" %>

<!-- Body -->

<body bgcolor="white">

<!-- Tree Component -->

<td width="200">
  <controls:tree tree="treeControlTest"
               action="treeControlTest.do?tree=${name}"
                style="tree-control"
        styleSelected="tree-control-selected"
      styleUnselected="tree-control-unselected"
  />

</body>

<!-- Standard Footer -->

<%@ include file="footer.jsp" %>

</html:html>
