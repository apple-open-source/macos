/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: FlashNamespaceHandler.java,v 1.5 2002/06/24 17:14:00 cazzius Exp $

package org.jboss.net.axis.server;

import org.apache.axis.AxisFault;
import org.apache.axis.MessageContext;
import org.apache.axis.Message;
import org.apache.axis.message.SOAPEnvelope;
import org.apache.axis.handlers.BasicHandler;

import org.jboss.net.DefaultResourceBundle;
import org.jboss.logging.Logger;
import org.apache.log4j.NDC;
import org.apache.log4j.Category;
import org.apache.log4j.Priority;

import java.io.PrintWriter;

/**
 * This class implements the Apache Axis Handler interface.  As such, it is
 * inserted into the chain of Axis Engine Handlers by specifying it in the
 * axis-config.xml file of the jboss-net-flash.sar file.  This particular
 * handler flags the generated SOAP Envelope to not include namespace
 * declarations.  This is because Flash versions 5 and MX do not support
 * namespaces and only support simple strings.
 * <br>
 * <h3>Change notes</h3>
 *   <ul>
 *   </ul>
 * @created  22.04.2002
 * @author <a href="mailto:fbrier@multideck.com">Frederick N. Brier</a>
 * @version $Revision: 1.5 $
 */

public class FlashNamespaceHandler extends BasicHandler {
	/**
	 * The instance logger for the service.  Not using a class logger
	 * because we want to dynamically obtain the logger name from
	 * concrete sub-classes.
	 */
//	protected Logger log;

//	public FlashNamespaceHandler()
//	{
//		super();

//		log = Logger.getLogger( getClass() );
//		log.trace( "Constructing" );
//	}

	/**
		* Implements 
		* @see Handler#invoke( MessageContext )
	*/
	public void invoke(MessageContext msgContext)
	{
		// log.trace("FlashNamespaceHandler.invoke(): Entered.");
		removeNamespaces(msgContext);
	}


	/*
		* @see Handler#onFault(MessageContext)
	*/
	public void onFault(MessageContext msgContext)
	{
//		log.error( "FlashNamespaceHandler.onFault(): Entered." );
	}


	/**
		* Flag the SOAP envelope not to use namespaces.
	*/
	protected void removeNamespaces(MessageContext msgContext)
	{
		Message msg = msgContext.getResponseMessage();
		try
			{
			SOAPEnvelope soapEnvelope = msg.getSOAPEnvelope();
			boolean result = soapEnvelope.removeNamespaceDeclaration("SOAP-ENV");
			}
		catch (AxisFault e)
			{
			}
	} // of method removeNamespaces

} // of class FlashNamespaceHandler
