<%@ page session="true" import="java.util.*,org.jboss.test.bench.servlet.*" %>
<jsp:useBean id="conf" class="org.jboss.test.bench.servlet.ConfigData" scope="session"/>
<% FullTester result = (FullTester)request.getAttribute("fullTester"); %>

<html>
  <head>
    <title>Test results</title>
  </head>
  <body bgcolor=#ffffff text=#000000>
  <h1>Full Server Test Results</h1>

  <h3>Configuration</h3>
  <table>
    <% for (int i = 0; i < conf.size(); i++) { %>
      <tr>
        <td align=right><%= conf.getName(i) %></td>
        <td><%= conf.getValue(i) %></td>
      </tr>
    <% } %>
  </table>

  <h3>Results</h3>

  <table border=2>
	<tr><th rowspan=2>Test Description
        <th colspan=<%= result.depth %>>Number of concurrent clients
    <tr>
	   <% for (int i=0; i<result.depth; i++) { %><th align=center><%= result.nbClients[i] %><% } %>
    
	
	<% for (int i=0; i<result.nbTests; i++) { %>
	<tr><td><%= result.getTestName(i) %>
	  <% for (int j=0; j<result.depth; j++) { %>
        <td align=center><%= result.getTestResult(i,j) %>
      <% } %>
    <% } %>
  </table>
	   

  </body>
</html>
