/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
*/
package org.jboss.net.taglib;

import java.util.Map;
import java.util.Map.Entry;
import java.util.Iterator;

import java.net.URL;

import java.io.IOException;

import javax.servlet.http.HttpServletRequest;
import javax.servlet.jsp.tagext.TagSupport;
import javax.servlet.jsp.JspTagException;
import javax.servlet.jsp.JspWriter;

import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.jboss.mx.util.MBeanServerLocator;

/**
	* JSP Tag class for FlashParameters JSP tag.  The purpose is to pass
	* information to a Flash program embedded in a web page so that needed
	* initial data is available to it.  Two types of information are passed, one
	* set is the hostUrl and the serviceContext of the SOAP server.  The second
	* is a collection of variable name, value pairs that would also be useful to
	* the Flash program.  When a web page contains a Flash program (.swf) file,
	* it specifies an &lt;OBJECT&gt; tag.  Inside the &lt;OBJECT&gt; tag is a parameter such
	* as:<br><br>
	*
	*    <p><code>&lt;PARAM NAME=movie VALUE="HelloWorldForm.swf"&gt;</p></code>
	*
	* Using a Macromedia specified technique for passing parameters to Flash, we
	* place at the end of the VALUE attribute, the FlashParameter tag:<br><br>
	*
	*    <p><code>...m.swf&lt;flash:flashparms/&gt;"&gt;</p></code>
	*
	* The instantiated tag becomes:<br><br>
	*
	*    <p><code>?hostUrl=http://www.yourhost.com:8080&amprootContext=axis</p></code>
	*
	* The tag attribute mbeanName, if not specified, has a default value of
	* "<code>jboss.net:service=Axis</code>"<br><br>
	*
	* As an added feature, by specifying a Bean, using the Tag "parms" attribute
	* that implements the java.util.Map interface the set of key/value pairs are
	* added to the Flash movie parameter list.
	*
	* @created 30. May 2002
	* @author <a href="mailto:fbrier@multideck.com">Frederick N. Brier</a>
	* @version $Revision: 1.4.2.1 $
	*
	* @jsp:tag  name="flashparms"
	*           body-content="empty"
	*           display-name="Flash Movie Parameters"
	*           description="Provide support for Flash SOAP and parameter passing"
	*
*/
public class FlashParametersTag extends TagSupport
{
	final public static String DEFAULT_JBOSSNET_MBEAN_NAME = "jboss.net:service=Axis";

	private String				mbeanName = null;
	private Map					parms = null;


	public FlashParametersTag()
	{
		super();
		
	}

	/**
		* Gets the Flash AxisService MBean name.
		*
		* @jsp:attribute	required="false" rtexprvalue="false"
		*                type="java.lang.String"
		*                description="MBean Name of the Flash AxisService MBean"
	*/
	public String getMbeanName()
	{
		return this.mbeanName;
	}

	public void setMbeanName( String mbeanName )
	{
		this.mbeanName = mbeanName;
	}

	/**
		* Gets additional parameters for Flash Movie.
		*
		* @jsp:attribute	required="false" rtexprvalue="true"
		*                type="java.util.Map"
		*                description="Map collection of parameters for the Flash Movie"
	*/
	public Map getParms()
	{
		return parms;
	}

	public void setParms( Map parms )
	{
		this.parms = parms;
	}
		

	public int doStartTag() throws JspTagException
	{
		HttpServletRequest request = (HttpServletRequest)pageContext.getRequest();

		try
			{
			JspWriter out = pageContext.getOut();
//			out.print( "?hostUrl=http://tornado.multideck.dnsq.org:8080&rootContext=/axis/services" );

			if ( null == mbeanName )
				{
				// Look for MBean using default values.
				mbeanName = DEFAULT_JBOSSNET_MBEAN_NAME;
				}

			MBeanServer	server = MBeanServerLocator.locateJBoss();
			ObjectName		jbossNetObjName = new ObjectName( mbeanName );

			String rootContext = (String)server.getAttribute( jbossNetObjName, "RootContext" );
			
//			String hostUrl = (String)server.getAttribute( jbossNetObjName, "HostUrl" );
			
			URL		reqUrl = new URL( request.getRequestURL().toString() );
			String	hostUrl = reqUrl.getHost() + ":" + reqUrl.getPort();

			out.print( "?hostUrl=" + hostUrl + "&rootContext=" + rootContext );
			if ( null != parms )
				{
				// Output any remaining parameters
				Iterator		iter = parms.entrySet().iterator();
				Map.Entry	curEntry;
				while ( iter.hasNext() )
					{
					curEntry = (Map.Entry)iter.next();
					out.print( "&" + curEntry.getKey().toString().trim() + "=" + curEntry.getValue().toString().trim() );
					}
				}   // if - there are map entries to output as parameters

			}
		catch( Exception e )
			{
			e.printStackTrace();
			throw new JspTagException( e.toString() );
			}

		return SKIP_BODY;
	}

}   // of class FlashParametersTag
