<%@ page session="true" import="java.util.*,org.jboss.test.bench.servlet.*" %>

<html>
  <head>
	<title>EJB Benchmark</title>
  </head>

  <body bgcolor=#ffffff text=#000000>
    <h1>EJB Benchmark v0.1</h1>
    <p>Welcome to jboss server benchmark suite. You can:</p><ul>
      <li><a href="#ejb">Test the EJB server alone</a>
      <li><a href="#full">Test the full server</a> (http server, servlet container, EJB server)
    </ul></p>
    <form action="EJBServerTest" method=get name=f>

      <h3><a name="ejb">Test the EJB server</a></h3>
      <p>This will test only the response times for the EJB server. A servlet will perform a number of operations on beans, compute the results and print the result.</p>

      <p>
         <table>
			<tr><td>Maximum number of concurrent clients: <td><select name=maxClients>
				<% for (int i=0; i< EJBTester.nbClients.length; i++) { %>
                  <option value="<%= EJBTester.nbClients[i] %>"><%= EJBTester.nbClients[i] %></option>
                <% } %> </select>
    	    <tr><td>Maximum number of calls (creations/invocations): <td><input type=text name=nbCalls value="1000"><br>
         </table>
         <table>
           <tr><td><input type=checkbox name=createSimpleEntity checked>   <td>Test simple entity creation
           <tr><td><input type=checkbox name=createComplexEntity checked>  <td>Test complex entity creation
           <tr><td><input type=checkbox name=readEntity checked>           <td>Invoke a read-only method on an entity
           <tr><td><input type=checkbox name=writeEntity checked>         <td>Write data on an entity creation
           <tr><td><input type=checkbox name=callSF checked>               <td>Call a method on a stateful session bean
           <tr><td><input type=checkbox name=callSL checked>               <td>Call a method on a stateless session bean
         </table>


      </p>

      <input name=goejb type=submit value="Test it!">
      </p>

      <h3><a name="full">Test the full server</a></h3>
      <p>This test consists of a servlet testing the whole server. This servlet will call a page involving the web server, the servlet container, and the EJB server.</p>

      <table>
           <tr><td><input type=checkbox name=servlet    checked>      <td>Test servlet alone
           <tr><td><input type=checkbox name=servlet2SL checked>      <td>Test servlet calling stateless session
           <tr><td><input type=checkbox name=servlet2Entity checked>  <td>Test servlet creating entity
      </table>
      <input name=goall type=submit value="Test it!">
    </form>
  </body>

</html>
