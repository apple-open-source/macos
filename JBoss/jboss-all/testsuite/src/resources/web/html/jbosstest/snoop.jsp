<%@page import="java.io.*,javax.naming.*,javax.servlet.http.HttpUtils" %>
<html>
<%!
    private String initException;
    private String jndiEnvCtxInfo;

    public void jspInit()
    {
        StringBuffer tmp = new StringBuffer();
        try
        {
            InitialContext ctx = new InitialContext();
            Context envCtx = (Context) ctx.lookup("java:comp/env");
            list(envCtx, "", tmp, true);
            jndiEnvCtxInfo = tmp.toString();
        }
        catch(NamingException e)
        {
            formatException(tmp, e);
            initException = tmp.toString();
        }
    }

    private void list(Context ctx, String indent, StringBuffer buffer, boolean verbose)
    {
        ClassLoader loader = Thread.currentThread().getContextClassLoader();
        try
        {
             NamingEnumeration ne = ctx.list("");
             while( ne.hasMore() )
             {
                NameClassPair pair = (NameClassPair) ne.next();
                boolean recursive = false;
                boolean isLinkRef = false;
                try
                {
                    Class c = loader.loadClass(pair.getClassName());
                    if( Context.class.isAssignableFrom(c) )
                        recursive = true;
                    if( LinkRef.class.isAssignableFrom(c) )
                        isLinkRef = true;
                }
                catch(ClassNotFoundException cnfe)
                {
                }

                String name = pair.getName();
                buffer.append(indent +  " +- " + name);
                if( isLinkRef )
                {
                    try
                    {
                        LinkRef link = (LinkRef) ctx.lookupLink(name);
                        buffer.append("[link -> ");
                        buffer.append(link.getLinkName());
                        buffer.append(']');
                    }
                    catch(Throwable e)
                    {
                        e.printStackTrace();
                        buffer.append("[invalid]");
                    }
                }
                if( verbose )
                    buffer.append(" (class: "+pair.getClassName()+")");
                buffer.append('\n');
                if( recursive )
                {
                   try
                    {
                        Object value = ctx.lookup(name);
                        if( value instanceof Context )
                        {
                            Context subctx = (Context) value;
                            list(subctx, indent + " |  ", buffer, verbose);
                        }
                        else
                        {
                            buffer.append(indent + " |   NonContext: "+value);
                            buffer.append('\n');
                        }
                    }
                    catch(Throwable t)
                    {
                        buffer.append("Failed to lookup: "+name+", errmsg="+t.getMessage());
                        buffer.append('\n');
                    }
               }
            }
            ne.close();
        }
        catch(NamingException ne)
        {
            buffer.append("error while listing context "+ctx.toString () + ": " + ne.toString(true));
            formatException(buffer, ne);
        }
    }

    private void formatException(StringBuffer buffer, Throwable t)
    {
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        buffer.append("<pre>\n");
        t.printStackTrace(pw);
        buffer.append(sw.toString());
        buffer.append("</pre>\n");
    }

%>

<body bgcolor="white">
<h1> JNDI java:comp/env Context Info</h1>
<pre>
<%
    if( initException != null )
        out.println(initException);
    else
        out.println(jndiEnvCtxInfo);
%>
</pre>
<h1> Request Information </h1>
<font size="4">
JSP Request Method: <%= request.getMethod() %>
<br>
Request URL: <%= HttpUtils.getRequestURL(request) %>
<br>
Request URI: <%= request.getRequestURI() %>
<br>
Request Protocol: <%= request.getProtocol() %>
<br>
Servlet path: <%= request.getServletPath() %>
<br>
Path info: <%= request.getPathInfo() %>
<br>
Path translated: <%= request.getPathTranslated() %>
<br>
Query string: <%= request.getQueryString() %>
<br>
Content length: <%= request.getContentLength() %>
<br>
Content type: <%= request.getContentType() %>
<br>
Server name: <%= request.getServerName() %>
<br>
Server port: <%= request.getServerPort() %>
<br>
Remote user: <%= request.getRemoteUser() %>
<br>
Remote address: <%= request.getRemoteAddr() %>
<br>
Remote host: <%= request.getRemoteHost() %>
<br>
Authorization scheme: <%= request.getAuthType() %> 
<br>
Locale: <%= request.getLocale() %>
<hr>
The browser you are using is <%= request.getHeader("User-Agent") %>
<hr>
</font>
</body>
</html>
