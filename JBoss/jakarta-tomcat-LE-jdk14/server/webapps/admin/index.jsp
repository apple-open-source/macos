<!-- Standard Struts Entries -->

<%@ page language="java" contentType="text/html;charset=utf-8" %>
<%@ taglib uri="/WEB-INF/struts-bean.tld" prefix="bean" %>
<%@ taglib uri="/WEB-INF/struts-html.tld" prefix="html" %>
<%@ taglib uri="/WEB-INF/struts-logic.tld" prefix="logic" %>

<html:html locale="true">

<!-- Standard Content -->

<%@ include file="header.jsp" %>

<!-- Body -->
<frameset rows="117,685*" cols="*" frameborder="NO" border="3" framespacing="3">
  <frame name="banner" src='<%= response.encodeURL("banner.jsp") %>' scrolling="no" title="commit and logout banner">
  <frameset cols="300,*" frameborder="YES" border="2">
    <frame name="tree" src='<%= response.encodeURL("setUpTree.do") %>' scrolling="auto" title="application navigation tree">
    <frame name="content" src='<%= response.encodeURL("blank.jsp") %>' scrolling="auto" title="content editing">
  </frameset>
</frameset>

<!-- Standard Footer -->

<%@ include file="footer.jsp" %>

</html:html>
