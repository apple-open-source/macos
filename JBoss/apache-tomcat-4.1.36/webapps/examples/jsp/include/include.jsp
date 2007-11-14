<html>
<!--
  Copyright (c) 1999 The Apache Software Foundation.  All rights 
  reserved.
-->

<body bgcolor="white">

<font color="red">

<%@ page buffer="5kb" autoFlush="false" %>

<p>In place evaluation of another JSP which gives you the current time:

<%@ include file="foo.jsp" %>

<p> <jsp:include page="/jsp/include/foo.html" flush="true"/> by including the output of another JSP:

<jsp:include page="foo.jsp" flush="true"/>

:-) 

</html>
