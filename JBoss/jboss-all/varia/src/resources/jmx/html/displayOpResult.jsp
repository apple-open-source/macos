<?xml version="1.0"?>
<%@page contentType="text/html"
   import="java.net.*,
   		   java.beans.PropertyEditor,
   		   org.jboss.util.propertyeditor.PropertyEditors"
%>

<!DOCTYPE html 
    PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
    "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html>
<head>
   <title>Operation Results</title>
   <link rel="stylesheet" href="style_master.css" type="text/css" />
   <meta http-equiv="cache-control" content="no-cache" />
</head>
<body>

<jsp:useBean id='opResultInfo' class='org.jboss.jmx.adaptor.control.OpResultInfo' scope='request'/>

         <img src="images/logo.gif" align="right" border="0" alt="logo" />
         <h1>JMX MBean Operation Result</h1>
         <h3>Operation <code><%= opResultInfo.name%>()</code></h3>

<p>
<a href='HtmlAdaptor?action=displayMBeans'>Back to Agent View</a>
&emsp;
<a href='HtmlAdaptor?action=inspectMBean&amp;name=<%= URLEncoder.encode(request.getParameter("name")) %>'>Back to MBean View</a>
&emsp;
<a href=
<%
	out.print("'HtmlAdaptor?action=invokeOpByName");
	out.print("&amp;name=" + URLEncoder.encode(request.getParameter("name")));
	out.print("&amp;methodName=" + opResultInfo.name );

	for (int i=0; i<opResultInfo.args.length; i++)
    {
		out.print("&amp;argType=" + opResultInfo.signature[i]);
		out.print("&amp;arg" + i + "=" + opResultInfo.args[i]);
	}

	out.println("'>Reinvoke MBean Operation");
%>
</a>
</p>

<hr />
   <span class='OpResult'>
<%
   if( opResultInfo.result == null )
   {
%>
   Operation completed successfully without a return value.
<%
   }
   else
   {
      String opResultString = null;
      
      PropertyEditor propertyEditor = PropertyEditors.findEditor(opResultInfo.result.getClass());
      if(propertyEditor != null) {
         propertyEditor.setValue(opResultInfo.result);
         opResultString = propertyEditor.getAsText();
      } else {
         opResultString = opResultInfo.result.toString();
      }

      boolean hasPreTag = opResultString.startsWith("<pre>");
      if( hasPreTag == false )
         out.println("<pre>");
      out.println(opResultString);
      if( hasPreTag == false )
         out.println("</pre>");
   }
%>
   </span>
</body>
</html>
