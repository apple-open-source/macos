<%
   String nodeName = System.getProperty("jboss.server.name");

   session.setAttribute("init", "bla");

   response.addHeader("ProcessingNode", nodeName);
   org.jboss.logging.Logger.getLogger("HA-HTTP." + nodeName).info("Init");
   Cookie id = new Cookie("SessionId", request.getSession().getId());
   response.addCookie(id);
%><%=session.getId()%>