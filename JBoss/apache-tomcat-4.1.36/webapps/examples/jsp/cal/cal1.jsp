<HTML>
<!--  
  Copyright (c) 1999 The Apache Software Foundation.  All rights 
  reserved.
-->
<HEAD><TITLE> 
	Calendar: A JSP APPLICATION
</TITLE></HEAD>


<BODY BGCOLOR="white">

<%@ page language="java" import="cal.*" %>
<jsp:useBean id="table" scope="session" class="cal.TableBean" />

<%
	table.processRequest(request);
	if (table.getProcessError() == false) {
%>

<!-- html table goes here -->
<CENTER>
<TABLE WIDTH=60% BGCOLOR=yellow CELLPADDING=15>
<TR>
<TD ALIGN=CENTER> <A HREF=cal1.jsp?date=prev> prev </A>
<TD ALIGN=CENTER> Calendar:<%= table.getDate() %></TD>
<TD ALIGN=CENTER> <A HREF=cal1.jsp?date=next> next </A>
</TR>
</TABLE>

<!-- the main table -->
<TABLE WIDTH=60% BGCOLOR=lightblue BORDER=1 CELLPADDING=10>
<TR>
<TH> Time </TH>
<TH> Appointment </TH>
</TR>
<FORM METHOD=POST ACTION=cal1.jsp>
<%
	for(int i=0; i<table.getEntries().getRows(); i++) {
	   cal.Entry entr = table.getEntries().getEntry(i);	
%>
	<TR>
	<TD> 
	<A HREF=cal2.jsp?time=<%= entr.getHour() %>>
		<%= entr.getHour() %> </A>
	</TD>
	<TD BGCOLOR=<%= entr.getColor() %>>
	<% out.print(util.HTMLFilter.filter(entr.getDescription())); %>
	</TD> 
	</TR>
<%
	}
%>
</FORM>
</TABLE>
<BR>

<!-- footer -->
<TABLE WIDTH=60% BGCOLOR=yellow CELLPADDING=15>
<TR>
<TD ALIGN=CENTER>  <% out.print(util.HTMLFilter.filter(table.getName())); %> : 
		     <% out.print(util.HTMLFilter.filter(table.getEmail())); %> </TD>
</TR>
</TABLE>
</CENTER>

<%
	} else {
%>
<font size=5>
	You must enter your name and email address correctly.
</font>
<%
	}
%>


</BODY>
</HTML>




