<%@ taglib uri="/webconsole" prefix="jb" %>
<jb:mbean id="module" intf="org.jboss.management.j2ee.J2EEModuleMBean" />

<%
   String moduleName = module.getObjectName().getKeyProperty ("name");
%>
<html>
<META HTTP-EQUIV="expires" CONTENT="0"/>
<head>
<title>EJB-Module: <%=moduleName%></title>
</head>

  <body>
  
  <h1><center>EJB-Module '<%=moduleName%>'</center></h1>
  
  <p/>
  <p/>
  
  <table border="1">
   <tr>
      <td><b>Management Object Name:</b></td>
   </tr>
   <tr>
      <td><%=module.getObjectName()%></td>
   </tr>
   <tr>
      <td><b>Provides Statistics:</b></td>
   </tr>
   <tr>
      <td><%=module.isStatisticsProvider()%></td>
   </tr>
   <tr>
      <td><b>Deployment Descriptor:</b></td>
   </tr>
   <tr>
      <td><pre><%=org.jboss.console.plugins.helpers.servlet.ServletHelper.filter(module.getDeploymentDescriptor())%></pre></td>
   </tr>
  </table>

  </body>

</html>
