/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.net.axis.server;

import java.io.IOException;

import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpServletRequestWrapper;

import org.apache.axis.transport.http.HTTPConstants;

import org.jboss.net.DefaultResourceBundle;
import org.jboss.logging.Logger;
import org.jboss.logging.Log4jLoggerPlugin;
import org.apache.log4j.NDC;
import org.apache.log4j.Category;
import org.apache.log4j.Priority;

import java.io.PrintWriter;

/**
 * A AxisServlet that allows the Flash player/plugin to interact with the
 * Axis SOAP engine despite the inability to generate the SOAPAction HTTP
 * header.  It spoofs the header by looking at the request parameters and
 * generating a derived HttpServletRequestWrapper class to appear to migrate
 * those that should actually be HTTP headers into the header.  This class
 * then just calls its base class's implementation of doPost().
 *
 * For example, if you were invoking the Hello World SOAP example, you would
 * append:
 *
 *    ?SOAPAction=\"Hello\"
 * 
 * to the service context:
 *
 *    /axisflash/flashservices/Hello
 *
 * <h3> Change History </h3>
 * <ul>
 * <li> jung, 02.05.2002: Outsourced the strings. </li>
 * </ul>
 * @author <a href="mailto:fbrier@multideck.com">Frederick N. Brier</a>
 * @created 22.04.2002
 * @version $Revision: 1.2 $
*/
public class FlashAxisServiceServlet extends AxisServiceServlet {
	/**
	 * This is a spoofing class whose sole purpose is to make it appear that
	 * the HTTP "SOAPAction" parameter is actually an HTTP Header attribute.
	 */
	public class FilteredHttpServletRequest extends HttpServletRequestWrapper {
		/** we keep the parameter here */
		protected String soapAction;

		/**
		 * Constructs an instance with the soapAction set to the value of the
		 * HTTP "SOAPAction" parameter.
		 * @param req - HttpServletRequest that is getting spoofed
		 * @exception IllegalArgumentException is thrown if either the request
		 * already had a SOAPAction header or if there is no SOAPAction
		 * parameter.
		 */
		public FilteredHttpServletRequest(HttpServletRequest req)
			throws IllegalArgumentException {
			super(req);

			soapAction = (String) req.getHeader(HTTPConstants.HEADER_SOAP_ACTION);

			if (null != soapAction) {
				// SOAPAction provided.  Don't need spoofed request.
				log.error(Constants.ERR_EXISTING_HEADER);
				throw new IllegalArgumentException(Constants.ERR_EXISTING_HEADER);
			}

			soapAction = getParameter(HTTPConstants.HEADER_SOAP_ACTION);

			if (null == soapAction) {
				log.error(Constants.ERR_MISSING_PARM);
				throw new IllegalArgumentException(Constants.ERR_MISSING_PARM);
			}

			log.trace("FilteredHttpServletRequest.ctor(): Matched SOAPAction parameter");

		} // of FilteredHttpServletRequest constructor

		/**
		 * If there is a SOAPAction HTTP parameter, return that value instead of NULL.
		 * Otherwise, default to the base class behavior.
		 * @param name - a String specifying the header name
		 * @returns a String containing the value of the SOAPAction HTTP parameter if
		 * that is what is requested via the name parameter, otherwise returns the
		 * expected HTTP Header value, or null if the request does not have a header
		 * of that name
		 */
		
		public String getHeader(String name) {
			log.trace("FlashAxisServiceServlet.FilteredHttpServletRequest.getHeader()");
			if (name.equals(HTTPConstants.HEADER_SOAP_ACTION)) {
				log.trace("getHeader(): Matched SOAPAction header request");
				return soapAction;
			} else {
				log.trace(
					"getHeader(): Not a SOAPAction header request, called base class method.");
				return super.getHeader(name);
			}
		} // of method getHeader

	} // of class FilteredHttpServletRequest

	/**
	 * The instance logger for the service.  Not using a class logger
	 * because we want to dynamically obtain the logger name from
	 * concrete sub-classes.
	 */
	protected Logger log;

	/**
		* Creates new AxisServlet
	*/
	public FlashAxisServiceServlet() {
		super();

		log = Logger.getLogger(getClass());
		log.trace("Constructing");

		// we fake internationalisation if it is not globally catered
      Category cat = ((Log4jLoggerPlugin)log.getLoggerPlugin ()).getCategory ();      
		if (null == cat.getResourceBundle())
			cat.setResourceBundle(new DefaultResourceBundle());
	}

	/**
		* This method sits on top of the AxisService.doPost() acting as a filter
		* by first creating a FilterHttpServletRequest and passing it to the base
		* class implementation.
		* @param req - an HttpServletRequest object that contains the request the
		* client has made of the servlet
		* @param resp - an HttpServletResponse object that contains the response
		* the servlet sends to the client
		* @exception IOException if an input or output error is detected when the
		* servlet handles the request
		* @exception ServletException	if the request for the POST could not be
		* handled
	*/
	public void doPost(HttpServletRequest req, HttpServletResponse res)
		throws ServletException, IOException {
		HttpServletRequest newReq = req;
		try {
			newReq = new FilteredHttpServletRequest(req);
			log.trace(
				"doPost(): Successfully created a FilteredHttpServletRequest object.");
		} catch (IllegalArgumentException e) {
			log.error(
				"doPost(): Failed to create a FilteredHttpServletRequest object.  Use original HttpServletRequest.");
		}

		super.doPost(newReq, res);
	}

}