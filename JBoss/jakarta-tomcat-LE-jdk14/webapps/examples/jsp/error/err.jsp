<html>
<!--
  Copyright (c) 1999 The Apache Software Foundation.  All rights 
  reserved.
-->
<body bgcolor="lightblue">

	<%@ page errorPage="errorpge.jsp" %>
	<jsp:useBean id="foo" scope="request" class="error.Smart" />
	<% 
		String name = null;

		if (request.getParameter("name") == null) {
	%>
	<%@ include file="/jsp/error/error.html" %>
	<%
		} else {
		  foo.setName(request.getParameter("name"));
		  if (foo.getName().equalsIgnoreCase("integra"))
		  	name = "acura";
		  if (name.equalsIgnoreCase("acura")) {
	%>

	<H1> Yes!!! <a href="http://www.acura.com">Acura</a> is my favorite car.

	<% 
		  }
		}	
	%>	
</body>
</html>

