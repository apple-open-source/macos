<%@page contentType="text/xml"
   import="java.io.File"
%>
<directory>
<%
   if (System.getProperty ("jboss.netboot.served.config") == null)
      System.setProperty ("jboss.netboot.served.config", "default");

   String location = getServletConfig().getInitParameter("directory");
   location = org.jboss.util.Strings.replaceProperties (location);
   
   String subDir = request.getParameter("dir");
   if (subDir != null)
      location = location + "/" + subDir;
   
   File base = new File(location);
   File[] subfolders = base.listFiles();
   for (int i=0; i<subfolders.length; i++)
   {
      File subfolder = subfolders[i];
      String name = subfolder.getName ();
      boolean isDirectory = subfolder.isDirectory ();
      long lastModified = subfolder.lastModified ();
      long size = subfolder.length ();
      if (isDirectory)
      {
%>
  <sub-directory>
    <name><%=name%></name>
    <modified><%=lastModified%></modified>
  </sub-directory>
<%
      }
      else
      {
%>
  <file>
    <name><%=name%></name>
    <modified><%=lastModified%></modified>
    <size><%=size%></size>
  </file>
<%
      }

   }
%>
</directory>

