<html>
<!-- 
  Copyright (c) 1999 The Apache Software Foundation.  All rights 
  reserved.
-->
<title> Plugin example </title>
<body bgcolor="white">
<h3> Current time is : </h3>
<jsp:plugin type="applet" code="Clock2.class" codebase="/examples/jsp/plugin/applet" jreversion="1.2" width="160" height="150" >
    <jsp:fallback>
        Plugin tag OBJECT or EMBED not supported by browser.
    </jsp:fallback>
</jsp:plugin>
<p>
<h4>
<font color=red> 
The above applet is loaded using the Java Plugin from a jsp page using the
plugin tag.
</font>
</h4>
</body>
</html>
