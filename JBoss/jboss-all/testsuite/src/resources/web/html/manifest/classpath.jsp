<!-- Tests of JSP page class access
-->
<%@page contentType="text/html"
   import="java.io.*,java.security.*,javax.naming.*,org.jboss.test.web.util.*"
%>
<%!
   private String formatException(Throwable t)
   {
      StringWriter sw = new StringWriter();
      PrintWriter pw = new PrintWriter(sw);
      t.printStackTrace(pw);
      return sw.toString();
   }
%>

<html>
<head><title>JSP Debug Page</title></head>
<body>

<h2>Classpath Access Tests</h2>
<h3>Access WEB-INF/classes/org/jboss/test/web/util/Util.class</h3>
<pre>
<%
   // Access a class from the WEB-INF/classes directory
   try
   {
      Class clazz = Class.forName("org.jboss.test.web.util.Util");
      out.println("Successfully loaded class: "+clazz.getName());
      ClassLoader cl = clazz.getClassLoader();
      ProtectionDomain pd = clazz.getProtectionDomain();
      CodeSource cs = pd.getCodeSource();
      out.println("  ClassLoader : "+cl.getClass().getName()+':'+cl.hashCode());
      out.println("  CodeSource.location : "+cs.getLocation());
      Class clazz2 = org.jboss.test.web.util.Util.class;
      if( clazz2.equals(clazz) == false )
         throw new ServletException("ClassCastException for ClassInClasses.class");
      out.println("Static org.jboss.test.web.util.Util.class matches Class.forName");
   }
   catch(Exception e)
   {
      out.println("Failed");
      String dump = formatException(e);
      out.println(dump);
   }
%>
</pre>


<jsp:useBean id="util" scope="session" class="org.jboss.test.web.util.Util" />
<h2>JSP ClassLoaders</h2>
<pre>
<%
   try
   {
      ClassLoader loader = Thread.currentThread().getContextClassLoader();
      String loaders = util.displayClassLoaders(loader);
      out.println(loaders);
   }
   catch(Exception e)
   {
      throw new ServletException();
   }
 %>
</pre>

</body>
</html>
