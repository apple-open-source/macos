<html>
<!--
  Copyright (c) 1999-2001 The Apache Software Foundation.  All rights 
  reserved.
-->
<body>
<%@ taglib uri="http://jakarta.apache.org/tomcat/examples-taglib" prefix="eg"%>

Radio stations that rock:

<ul>
<eg:foo att1="98.5" att2="92.3" att3="107.7">
<li><%= member %></li>
</eg:foo>
</ul>

<eg:log>
Did you see me on the stderr window?
</eg:log>

<eg:log toBrowser="true">
Did you see me on the browser window as well?
</eg:log>

</body>
</html>
