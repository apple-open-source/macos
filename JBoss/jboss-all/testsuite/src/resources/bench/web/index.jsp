<%@ page session="true" import="java.util.*" %>
<jsp:useBean id="conf" class="org.jboss.test.bench.servlet.ConfigData" scope="session"/>


<html>
  <head>
	<title>EJB Benchmark</title>
  </head>

  <body bgcolor=#ffffff text=#000000>
    <h1>EJB Benchmark v0.1</h1>
    <form action="servlet/EJBServerTest" method=get name=f>

      <h3><a name="info">Information about your system</a></h3>
      <p>Please enter the following information about your system. This information will be printed on the result page.</p>
      <table>
        <% for (int i=0; i < conf.size(); i++) { %>
          <tr>
            <td align=right><%= conf.getName(i) %></td>
            <td><input type=text value="<%= conf.getValue(i) %>" framewidth=4 name="<%= conf.getName(i) %>" size=25></td>
          </tr>
        <% } %>
      </table>

      <input name=gototest type=submit value="Proceed to the tests">
    </form>
  </body>

</html>
