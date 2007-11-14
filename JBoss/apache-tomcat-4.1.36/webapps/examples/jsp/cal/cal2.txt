<HTML>
<!--  
  Copyright (c) 1999 The Apache Software Foundation.  All rights 
  reserved.
-->

<HEAD><TITLE> 
	Calendar: A JSP APPLICATION
</TITLE></HEAD>


<BODY BGCOLOR="white">
<jsp:useBean id="table" scope="session" class="cal.TableBean" />

<% 
	String time = request.getParameter ("time");
%>

<FONT SIZE=5> Please add the following event:
<BR> <h3> Date <%= table.getDate() %>
<BR> Time <%= time %> </h3>
</FONT>
<FORM METHOD=POST ACTION=cal1.jsp>
<BR> 
<BR> <INPUT NAME="date" TYPE=HIDDEN VALUE="current">
<BR> <INPUT NAME="time" TYPE=HIDDEN VALUE=<%= time %>
<BR> <h2> Description of the event <INPUT NAME="description" TYPE=TEXT SIZE=20> </h2>
<BR> <INPUT TYPE=SUBMIT VALUE="submit">
</FORM>

</BODY>
</HTML>

