<%@page contentType="text/html"
   import="javax.naming.*,org.jboss.test.web.interfaces.*"
%>
<%!

   private String callEJB(String arg) throws ServletException
   {
      String value = null;
      try
      {
         InitialContext ctx = new InitialContext();
         Object ref = ctx.lookup("java:comp/env/ejb/SecuredEJB");
         StatelessSessionHome home = (StatelessSessionHome) ref;
         StatelessSession bean = home.create();
         String echoValue = bean.echo(arg);
         value = "SecuredEJB.echo("+arg+") returned: "+echoValue;
         value += "\nCodeBase: "+home.getClass().getProtectionDomain().getCodeSource();
      }
      catch(Exception e)
      {
         throw new ServletException(e);
      }
      return value;
   }
   private String callLocalEJB(String arg) throws ServletException
   {
      String value = null;
      try
      {
         InitialContext ctx = new InitialContext();
         Object ref = ctx.lookup("java:comp/env/ejb/local/SecuredEJB");
         StatelessSessionLocalHome home = (StatelessSessionLocalHome) ref;
         StatelessSessionLocal bean = home.create();
         String echoValue = bean.echo(arg);
         value = "SecuredEJBLocal.echo("+arg+") returned: "+echoValue;
         value += "\nCodeBase: "+home.getClass().getProtectionDomain().getCodeSource();
      }
      catch(Exception e)
      {
         throw new ServletException(e);
      }
      return value;
   }
%>

<html>
<head><title>JSP to EJB Test</title></head>
<body>

<h1>JSP to EJB Test Via Remote Interface</h1>
<pre>
<%
   String arg = request.getParameter("name");
   String reply = callEJB(arg);
   out.println(reply);
   out.flush();
%>
</pre>
<h1>JSP to EJB Test Via Local Interface</h1>
<pre>
<%
   arg = request.getParameter("name");
   reply = callLocalEJB(arg);
   out.println(reply);
   out.flush();
%>
</pre>

</body>
</html>
