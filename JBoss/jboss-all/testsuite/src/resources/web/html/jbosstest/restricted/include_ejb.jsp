<%@page contentType="text/html"%>

<html>
<head><title>JSP Include ejb.jsp Test</title></head>
<body>

<jsp:include page="ejb.jsp"  flush="true">
   <jsp:param name="name" value="d1"/> 
</jsp:include>
<jsp:include page="ejb.jsp"  flush="true">
   <jsp:param name="name" value="d2"/> 
</jsp:include>
<jsp:include page="ejb.jsp"  flush="true">
   <jsp:param name="name" value="d3"/> 
</jsp:include>
<jsp:include page="ejb.jsp"  flush="true">
   <jsp:param name="name" value="d4"/> 
</jsp:include>

</body>
</html>
