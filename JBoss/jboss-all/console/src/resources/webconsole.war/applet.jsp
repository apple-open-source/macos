<html>

<head>
</head>

<body>

<p>
<applet 
   width="100%" 
   height="100%" 
   code="org.jboss.console.navtree.AppletBrowser"
   archive="applet.jar"
   >
   <param name="RefreshTime" value="5">
   <param name="SessionId" value="<%=request.getSession().getId()%>">
   <param name="PMJMXName" value="jboss.admin:service=PluginManager">
</applet>
</p>

</body>

</html>
