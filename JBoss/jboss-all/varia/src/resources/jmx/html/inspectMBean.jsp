<?xml version="1.0"?>
<%@page contentType="text/html"
   import="java.net.*,java.util.*,javax.management.*,javax.management.modelmbean.*,
   org.jboss.jmx.adaptor.control.Server,
   org.jboss.jmx.adaptor.control.AttrResultInfo,
   org.jboss.jmx.adaptor.model.*,
   org.jdom.output.XMLOutputter,
   java.lang.reflect.Array"
%>
<%! 
    XMLOutputter xmlOutput = new XMLOutputter();
    String sep = System.getProperty("line.separator", "\n");

    public String fixDescription(String desc)
    {
      if (desc == null || desc.equals(""))
      {
        return "(no description)";
      }
      return desc;
    }

    public String fixValue(Object value)
    {
        if (value == null)
            return null;
        String s = String.valueOf(value);
        return xmlOutput.escapeElementEntities(s);
    }

    public String fixValueForAttribute(Object value)
    {
        if (value == null)
            return null;
        String s = String.valueOf(value);
        return xmlOutput.escapeAttributeEntities(s);
    }
%>

<!DOCTYPE html 
    PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN"
    "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html>
<head>
   <title>MBean Inspector</title>
   <link rel="stylesheet" href="style_master.css" type="text/css" />
   <meta http-equiv="cache-control" content="no-cache" />
</head>
<body>

<jsp:useBean id='mbeanData' class='org.jboss.jmx.adaptor.model.MBeanData' scope='request'/>

<%
   ObjectName objectName = mbeanData.getObjectName();
   String objectNameString = mbeanData.getName();
   MBeanInfo mbeanInfo = mbeanData.getMetaData();
   MBeanAttributeInfo[] attributeInfo = mbeanInfo.getAttributes();
   MBeanOperationInfo[] operationInfo = mbeanInfo.getOperations();
%>

<img src="images/logo.gif" align="right" border="0" alt="" />

<h1>JMX MBean View</h1>

<%
   Hashtable properties = objectName.getKeyPropertyList();
   int size = properties.keySet().size();
%>
   <table class="ObjectName" cellspacing="0" cellpadding="5">
   <tr>
      <td class="nameh" rowspan="<%= size + 1 %>">Name</td>
      <td class='sep'>Domain</td>
      <td class='sep'><%= objectName.getDomain() %></td>
   </tr>
<%
   Iterator it = properties.keySet().iterator();
   while( it.hasNext() )
   {
      String key = (String) it.next();
      String value = (String) properties.get( key );
%>
   <tr>
      <td class='sep'><%= key %></td>
      <td class='sep'><%= value %></td>
   </tr>
<%
   }
%>
   <tr>
      <td class='nameh'>Java Class</td>
      <td colspan="3"><jsp:getProperty name='mbeanData' property='className'/></td></tr>
      <tr><td class='nameh'>Description</td>
      <td colspan="3" class="adescription">
         <%= fixDescription(mbeanInfo.getDescription())%>
      </td>
   </tr>
</table>

<p>
<a href='HtmlAdaptor?action=displayMBeans'>Back to Agent View</a>
&emsp;
<a href='HtmlAdaptor?action=inspectMBean&amp;name=<%= URLEncoder.encode(request.getParameter("name")) %>'>Refresh MBean View</a>
</p>

<form method="post" action="HtmlAdaptor">
   <input type="hidden" name="action" value="updateAttributes" />
   <input type="hidden" name="name" value="<%= objectNameString %>" />
   <table class="AttributesClass" cellspacing="0" cellpadding="5">
      <tr class="AttributesHeader">
      <th width="25%">
         <span class='aname'>Attribute Name</span> <span class='aaccess'>(Access)</span><br/>
         <span class='atype'>Type</span> <br/>
         <span class='adescription'>Description</span> <br/>
      </th>
      <th width="75%" class='aname'>Attribute Value</th>
      </tr>
<%
   boolean hasWriteable = false;
   for(int a = 0; a < attributeInfo.length; a ++)
   {
      MBeanAttributeInfo attrInfo = attributeInfo[a];
      String attrName = attrInfo.getName();
      String attrType = attrInfo.getType();
      AttrResultInfo attrResult = Server.getMBeanAttributeResultInfo(objectNameString, attrInfo);
      String attrValue = attrResult.getAsText();
      String access = "";
      if( attrInfo.isReadable() )
         access += "R";
      if( attrInfo.isWritable() )
      {
         access += "W";
         hasWriteable = true;
      }
      String attrDescription = fixDescription(attrInfo.getDescription());
%>
		<tr>
		  <td class='sep'>
           <span class='aname'><%= attrName %></span> <span class='aaccess'>(<%= access %>)</span> <br/>
           <span class='atype'><%= attrType %></span> <br/>
           <span class='adescription'><%= attrDescription %></span> <br/>
        </td>
        <td class='sep'>
<%
      if( attrInfo.isWritable() )
      {
         String readonly = attrResult.editor == null ? "readonly='readonly'" : "";
         if( attrType.equals("boolean") || attrType.equals("java.lang.Boolean") )
         {
            // Boolean true/false radio boxes
            Boolean value = Boolean.valueOf(attrValue);
            String trueChecked = (value == Boolean.TRUE ? "checked='checked'" : "");
            String falseChecked = (value == Boolean.FALSE ? "checked='checked'" : "");
%>
            <input type="radio" name="<%= attrName %>" value="True" <%=trueChecked%>/>True
            <input type="radio" name="<%= attrName %>" value="False" <%=falseChecked%>/>False
<%
         }
         else if( attrInfo.isReadable() )
         {  // Text fields for read-write string values
            attrValue = fixValueForAttribute(attrValue);
            if (String.valueOf(attrValue).indexOf(sep) == -1)
            {
%>
            <input class="iauto" type="text" name="<%= attrName %>" value="<%= attrValue %>" <%= readonly %>/>
<%
            }
            else
            {
%>
            <textarea cols="80" rows="10" nowrap='nowrap' type="text" name="<%= attrName %>" <%= readonly %>><%= attrValue %></textarea>
<%
            }
         }
         else
         {  // Empty text fields for write-only
%>
		    <input class="iauto" type="text" name="<%= attrName %>" <%= readonly %>/>
<%
         }
      }
      else
      {
         if( attrType.equals("[Ljavax.management.ObjectName;") )
         {
            // Array of Object Names
            ObjectName[] names = (ObjectName[]) Server.getMBeanAttributeObject(objectNameString, attrName);
            if( names != null )
            {
%>
<%
               for( int i = 0; i < names.length; i++ )
               {
%>
                  <a href="HtmlAdaptor?action=inspectMBean&name=<%= URLEncoder.encode(( names[ i ] + "" )) %>"><%= ( names[ i ] + "" ) %></a>
                  <br />
<%
               }
            }
         }
         // Array of some objects
         else if( attrType.startsWith("["))
         {
            Object arrayObject = Server.getMBeanAttributeObject(objectNameString, attrName);
            if (arrayObject != null) {
               out.print("<pre>");
               for (int i = 0; i < Array.getLength(arrayObject); ++i) {
                  out.println(fixValue(Array.get(arrayObject,i)));
               }
               out.print("</pre>");
            } 
            
         }
         else
         {
            // Just the value string
%>
            <pre><%= fixValue(attrValue) %></pre>
<%
         }
      }
      if( attrType.equals("javax.management.ObjectName") )
      {
         // Add a link to the mbean
         if( attrValue != null )
         {
%>
            <br />
            <a href="HtmlAdaptor?action=inspectMBean&name=<%= URLEncoder.encode(attrValue) %>">View MBean</a>
<%
         }
      }
%>
         </td>
		</tr>
<%
   }
%>
<% if( hasWriteable )
   {
%>
   <tr>
      <td colspan='2'>
         <p align='center'>
            <input class='applyb' type="submit" value="Apply Changes" />
         </p>
      </td>
   </tr>
<%
   }
%>
   </table>
</form>

<p>
</p>

<% if (operationInfo.length > 0) { %>
<table class="AttributesClass" cellspacing="0" cellpadding="5">
   <tr class="AttributesHeader">
   <th width="25%">
      <span class='aname'>Operation Name</span><br/>
      <span class='atype'>Return Type</span> <br/>
      <span class='adescription'>Description</span> <br/>
   </th>
   <th width="75%">
      <span class='aname'>Parameters</span>
   </th>
   </tr>
<%
   for(int a = 0; a < operationInfo.length; a ++)
   {
      MBeanOperationInfo opInfo = operationInfo[a];
      boolean accept = true;
      if (opInfo instanceof ModelMBeanOperationInfo)
      {
         Descriptor desc = ((ModelMBeanOperationInfo)opInfo).getDescriptor();
         String role = (String)desc.getFieldValue("role");
         if ("getter".equals(role) || "setter".equals(role))
         {
            accept = false;
         }
      }
      if (accept)
      {
         MBeanParameterInfo[] sig = opInfo.getSignature();
%>
   <tr>
		<td class='sep'>
         <span class='aname'><%= opInfo.getName() %></span><br/>
         <span class='atype'><%= opInfo.getReturnType() %></span> <br/>
         <span class='adescription'><%= fixDescription(opInfo.getDescription())%></span> <br/>
      </td>
      <td class='sep'>
         <form method="post" action="HtmlAdaptor">
            <input type="hidden" name="action" value="invokeOp" />
            <input type="hidden" name="name" value="<%= objectNameString %>" />
            <input type="hidden" name="methodIndex" value="<%= a %>" />
<%
         if( sig.length > 0 )
         {
%>
	<table>
<%
            for(int p = 0; p < sig.length; p ++)
            {
               MBeanParameterInfo paramInfo = sig[p];
               String pname = paramInfo.getName();
               String ptype = paramInfo.getType();
               if( pname == null || pname.length() == 0 || pname.equals(ptype) )
               {
                  pname = "arg"+p;
               }
               String pdesc = fixDescription(paramInfo.getDescription());
%>
		<tr>
         <td>
         <span class='aname'><%= pname %></span><br/>
         <span class='atype'><%= ptype %></span> <br/>
         <span class='adescription'><%= pdesc %></span> <br/>
<%
                if( ptype.equals("boolean") || ptype.equals("java.lang.Boolean") )
                {
                   // Boolean true/false radio boxes
%>
            <input type="radio" name="arg<%= p%>" value="True" checked='checked'>True
            <input type="radio" name="arg<%= p%>" value="False" />False
<%
                 }
                 else
                 {
%>
            <input type="text" class="iauto" name="arg<%= p%>" />
<%
                  }
%>
         </td>
		</tr>
<%
               } // parameter list
%>
      </table>
<%
         } // has parameter list
%>
      <input type="submit" value="Invoke" />
      </form>

</td></tr>
<%
      } // mbean operation
   } // all mbean operations
%>
</table>
<%
} // has mbean operation
%>

</body>
</html>
