<html>
<!--
  Copyright (c) 1999 The Apache Software Foundation.  All rights 
  reserved.
-->

<jsp:useBean id="cart" scope="session" class="sessions.DummyCart" />

<jsp:setProperty name="cart" property="*" />
<%
	cart.processRequest(request);
%>


<FONT size = 5 COLOR="#CC0000">
<br> You have the following items in your cart:
<ol>
<% 
	String[] items = cart.getItems();
	for (int i=0; i<items.length; i++) {
%>
<li> <% out.print(util.HTMLFilter.filter(items[i])); %> 
<%
	}
%>
</ol>

</FONT>

<hr>
<%@ include file ="/jsp/sessions/carts.html" %>
</html>
