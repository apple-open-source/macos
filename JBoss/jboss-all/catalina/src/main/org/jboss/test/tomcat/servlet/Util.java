package org.jboss.test.tomcat.servlet;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.Date;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.LinkRef;
import javax.naming.NamingEnumeration;
import javax.naming.NamingException;
import javax.naming.NameClassPair;
import javax.naming.NameParser;

/** A utility class for generating ENC and class loader dumps.

@author  Scott.Stark@jboss.org
@version $Revision: 1.1 $
*/
public class Util
{

    public static void showTree(String indent, Context ctx, PrintWriter out)
        throws NamingException
    {
        ClassLoader loader = Thread.currentThread().getContextClassLoader();
        NamingEnumeration enum = ctx.list("");
        while( enum.hasMoreElements() )
        {
            NameClassPair ncp = (NameClassPair)enum.next();
            String name = ncp.getName();
            out.print(indent +  " +- " + name);
            boolean recursive = false;
            boolean isLinkRef = false;
            try
            {
                Class c = loader.loadClass(ncp.getClassName());
                if( Context.class.isAssignableFrom(c) )
                    recursive = true;
                if( LinkRef.class.isAssignableFrom(c) )
                    isLinkRef = true;
            }
            catch(ClassNotFoundException cnfe)
            {
            }

            if( isLinkRef )
            {
                try
                {
                    LinkRef link = (LinkRef) ctx.lookupLink(name);
                    out.print("[link -> ");
                    out.print(link.getLinkName());
                    out.print(']');
                }
                catch(Throwable e)
                {
                    e.printStackTrace();
                    out.print("[invalid]");
                }
            }
            out.println();

            if( recursive )
            {
               try
                {
                    Object value = ctx.lookup(name);
                    if( value instanceof Context )
                    {
                        Context subctx = (Context) value;
                        showTree(indent + " |  ", subctx, out);
                    }
                    else
                    {
                        out.println(indent + " |   NonContext: "+value);
                    }
                }
                catch(Throwable t)
                {
                    out.println("Failed to lookup: "+name+", errmsg="+t.getMessage());
                }
           }

        }
    }

    public static void dumpClassLoader(ClassLoader cl, PrintWriter out)
    {
        int level = 0;
        while( cl != null )
        {
            String msg = "Servlet ClassLoader["+level+"]: "+cl.getClass().getName()+':'+cl.hashCode();
            out.println(msg);
            if( cl instanceof URLClassLoader )
            {
                URLClassLoader ucl = (URLClassLoader) cl;
                URL[] urls = ucl.getURLs();
                msg = "  URLs:";
                out.println(msg);
                for(int u = 0; u < urls.length; u ++)
                {
                    msg = "  ["+u+"] = "+urls[u];
                    out.println(msg);
                }
            }
            cl = cl.getParent();
            level ++;
        }
    }

    public static void dumpENC(PrintWriter out) throws NamingException
    {
       InitialContext iniCtx = new InitialContext();
       Context enc = (Context) iniCtx.lookup("java:comp/env");
       showTree("", enc, out);
    }

    public static String displayClassLoaders(ClassLoader cl) throws NamingException
    {
       StringWriter sw = new StringWriter();
       PrintWriter out = new PrintWriter(sw);
       dumpClassLoader(cl, out);
       return sw.toString();
    }

    public static String displayENC() throws NamingException
    {
       StringWriter sw = new StringWriter();
       PrintWriter out = new PrintWriter(sw);
       dumpENC(out);
       return sw.toString();
    }
}
