<?xml version="1.0"?>
<%@page contentType="text/html"
   import="java.net.*,java.util.*,org.jboss.jmx.adaptor.model.*,java.io.*"
%>
<!DOCTYPE html 
    PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
    "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html>
<head>
   <title>JBoss JMX Management Console</title>
   <link rel="stylesheet" href="style_master.css" type="text/css" />
   <meta http-equiv="cache-control" content="no-cache" />
</head>
<body>
   <img src="images/logo.gif" align="right" border="0" alt="logo" />
   <%
   String hostname = "";
   try
   {
      hostname = InetAddress.getLocalHost().getHostName();
   }
   catch(IOException e)  {}
   %>
   <h1>JMX Agent View</h1>
   <h3><%= hostname %></h3>

<form action="HtmlAdaptor?action=displayMBeans" method="post" name="applyFilter" id="applyFilter">
ObjectName Filter (e.g. "jboss:*", "*:service=invoker,*"): <br />
<input type="text" name="filter" size="40" value="<%= request.getAttribute("filter")%>" />
  <br/>
<input type="submit" name="apply" value="Apply Filter" />
<input type="button" onClick="javascript:location='HtmlAdaptor?filter='" value="Clear Filter" />

<%
   Iterator mbeans = (Iterator) request.getAttribute("mbeans");
   while( mbeans.hasNext() )
   {
      DomainData domainData = (DomainData) mbeans.next();
%>
   <h2 class='DomainName'>
   <a href="javascript:document.applyFilter.filter.value='<%= domainData.getDomainName() %>:*';document.applyFilter.submit()"><%= domainData.getDomainName() %></a>
   </h2>
   <ul class='MBeanList'>
<%
      MBeanData[] data = domainData.getData();
      for(int d = 0; d < data.length; d ++)
      {
         String name = data[d].getObjectName().toString();
         String properties = data[d].getNameProperties();
%>
      <li><a href="HtmlAdaptor?action=inspectMBean&amp;name=<%= URLEncoder.encode(name) %>"><%= URLDecoder.decode(properties) %></a></li>
<%
      }
%>
   </ul>
<%
   }
%>

</form>
</body>
</html>
