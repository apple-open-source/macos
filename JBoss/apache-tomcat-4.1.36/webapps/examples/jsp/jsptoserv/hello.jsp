<html>
<!--
  Copyright (c) 1999 The Apache Software Foundation.  All rights 
  reserved.
-->
<body bgcolor="white">

<h1>
I have been invoked by
<% out.print (request.getAttribute("servletName").toString()); %>
Servlet.
</h1>

</html>