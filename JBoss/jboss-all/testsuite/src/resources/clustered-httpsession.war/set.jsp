<%
   String key = request.getParameter("key");
   String size = request.getParameter("size");
   int intSize = 64;
   String nodeName = System.getProperty("jboss.server.name");

   if (size != null)
      intSize = Integer.parseInt(size);


   if (key == null)
      key = "DEFAULT_KEY";

   session.setAttribute(key, new byte[intSize] );

   response.addHeader("ProcessingNode", nodeName);
   Cookie id = new Cookie("SessionId", request.getSession().getId());
   response.addCookie(id);
   org.jboss.logging.Logger.getLogger("HA-HTTP." + nodeName).info("Set: [" + key + "] : " + size);
%>