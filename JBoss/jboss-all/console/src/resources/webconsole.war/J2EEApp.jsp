<%@ taglib uri="/webconsole" prefix="jb" %>
<jb:mbean id="app" intf="org.jboss.management.j2ee.J2EEApplicationMBean" />

<%
   String appName = app.getObjectName().getKeyProperty ("name");
%>
<html>
<META HTTP-EQUIV="expires" CONTENT="0"/>
<head>
<title>J2EE Application: <%=appName%></title>
</head>

  <body>
  
  <h1><center>J2EE Application '<%=appName%>'</center></h1>
  
  <p/>
  <p/>
  
  <table border="1">
   <tr>
      <td><b>Management Object Name:</b></td>
   </tr>
   <tr>
      <td><%=app.getObjectName()%></td>
   </tr>
   <tr>
      <td><b>Provides Statistics:</b></td>
   </tr>
   <tr>
      <td><%=app.isStatisticsProvider()%></td>
   </tr>
   <tr>
      <td><b>Deployment Descriptor:</b></td>
   </tr>
   <tr>
      <td><pre><%=org.jboss.console.plugins.helpers.servlet.ServletHelper.filter(app.getDeploymentDescriptor())%></pre></td>
   </tr>
  </table>

  </body>

</html>
