<%
   String key = request.getParameter("key");
   int size = -1;
   String nodeName = System.getProperty("jboss.server.name");

   if (key == null)
      key = "DEFAULT_KEY";

   byte[] content = (byte[])session.getAttribute(key);
   if (content != null)
      size = content.length;

   Cookie id = new Cookie("SessionId", request.getSession().getId());
   response.addCookie(id);
   response.addHeader("ProcessingNode", nodeName);
   org.jboss.logging.Logger.getLogger("HA-HTTP." + nodeName).info("Get: [" + key + "] : " + size);
%><%=size%>