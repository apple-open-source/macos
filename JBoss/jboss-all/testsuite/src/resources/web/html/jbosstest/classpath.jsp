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
<h3>Access WEB-INF/classes/org/jboss/test/web/util/ClassInClasses.class</h3>
<pre>
<%
   // Access a class from the WEB-INF/classes directory
   try
   {
      Class clazz = Class.forName("org.jboss.test.web.util.ClassInClasses");
      out.println("Successfully loaded class: "+clazz.getName());
      ClassLoader cl = clazz.getClassLoader();
      ProtectionDomain pd = clazz.getProtectionDomain();
      CodeSource cs = pd.getCodeSource();
      out.println("  ClassLoader : "+cl.getClass().getName()+':'+cl.hashCode());
      out.println("  CodeSource.location : "+cs.getLocation());
      Class clazz2 = org.jboss.test.web.util.ClassInClasses.class;
      if( clazz2.equals(clazz) == false )
         throw new ServletException("ClassCastException for ClassInClasses.class");
      out.println("Static org.jboss.test.web.util.ClassInClasses.class matches Class.forName");
   }
   catch(Exception e)
   {
      out.println("Failed");
      String dump = formatException(e);
      out.println(dump);
   }
%>
</pre>
<h3>Access WEB-INF/lib/util.jar!org/jboss/test/web/util/Util.class</h3>
<pre>
<%
   // Access a class from a WEB-INF/lib jar
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
         throw new ServletException("ClassCastException for Util.class");
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

<h3>Access javax.management.j2ee.EJBStats from server/default/lib</h3>
<pre>
<%
   // Access a class from the server config lib directory
   try
   {
      Class clazz = Class.forName("javax.management.j2ee.EJBStats");
      out.println("Successfully loaded class: "+clazz.getName());
      ClassLoader cl = clazz.getClassLoader();
      ProtectionDomain pd = clazz.getProtectionDomain();
      CodeSource cs = pd.getCodeSource();
      out.println("  ClassLoader : "+cl.getClass().getName()+':'+cl.hashCode());
      out.println("  CodeSource.location : "+cs.getLocation());
      Class clazz2 = javax.management.j2ee.statistics.EJBStats.class;
      if( clazz2.equals(clazz) == false )
         throw new ServletException("ClassCastException for EJBStats.class");
      out.println("Static javax.management.j2ee.EJBStats.class matches Class.forName");
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
<h2>JSP View of JNDI ENC</h2>
<pre>
<%
   try
   {
      String enc = util.displayENC();
      out.println(enc);
   }
   catch(NamingException e)
   {
      throw new ServletException(e.toString(true));
   }
 %>
</pre>

</body>
</html>
