package org.jboss.jmx.adaptor.html;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Iterator;
import javax.management.AttributeList;
import javax.management.JMException;
import javax.servlet.RequestDispatcher;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;

import org.jboss.logging.Logger;
import org.jboss.jmx.adaptor.control.OpResultInfo;
import org.jboss.jmx.adaptor.control.Server;
import org.jboss.jmx.adaptor.model.MBeanData;

/** The HTML adaptor controller servlet.
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.4.2.5 $
 */
public class HtmlAdaptorServlet extends HttpServlet
{
   private static Logger log = Logger.getLogger(HtmlAdaptorServlet.class);
   private static final String ACTION_PARAM = "action";
   private static final String FILTER_PARAM = "filter";
   private static final String DISPLAY_MBEANS_ACTION = "displayMBeans";
   private static final String INSPECT_MBEAN_ACTION = "inspectMBean";
   private static final String UPDATE_ATTRIBUTES_ACTION = "updateAttributes";
   private static final String INVOKE_OP_ACTION = "invokeOp";
   private static final String INVOKE_OP_BY_NAME_ACTION = "invokeOpByName";

   /** Creates a new instance of HtmlAdaptor */
   public HtmlAdaptorServlet()
   {
   }
   
   public void init(ServletConfig config) throws ServletException
   {
      super.init(config);
   }

   public void destroy()
   {
   }
   
   protected void doGet(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException
   {
      processRequest(request, response);
   }
   protected void doPost(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException
   {
      processRequest(request, response);
   }

   protected void processRequest(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException
   {
      String action = request.getParameter(ACTION_PARAM);

      if( action == null )
         action = DISPLAY_MBEANS_ACTION;

      if( action.equals(DISPLAY_MBEANS_ACTION) )
         displayMBeans(request, response);
      else if( action.equals(INSPECT_MBEAN_ACTION) )
         inspectMBean(request, response);
      else if( action.equals(UPDATE_ATTRIBUTES_ACTION) )
         updateAttributes(request, response);
      else if( action.equals(INVOKE_OP_ACTION) )
         invokeOp(request, response);
      else if( action.equals(INVOKE_OP_BY_NAME_ACTION) )
         invokeOpByName(request, response);
   }

   /** Display all mbeans categorized by domain
    */
   private void displayMBeans(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException
   {
      // get ObjectName filter from request or session context 
	  HttpSession session = request.getSession(false);
      String      filter  = request.getParameter(FILTER_PARAM);

      if( filter == null && session != null)
      {
         // try using previously provided filter from session context
         filter = (String) session.getAttribute(FILTER_PARAM);   
      }
      
      if( filter != null && filter.length() > 0 )
      {
         // Strip any enclosing quotes
         if( filter.charAt(0) == '"' )
            filter = filter.substring(1);
         if( filter.charAt(filter.length()-1) == '"')
            filter = filter.substring(0, filter.length()-2);

		 // be a litte it tolerant to user input
         String domain = "*";
         String props  = "*,*";

		 int separator  = filter.indexOf(':');
         int assignment = filter.indexOf('=');

         if (separator == -1 && assignment != -1)
		 {
			// assume properties only
            props = filter.trim();
         }
		 else if (separator == -1 && assignment == -1)
         {
			// assume domain name only
			domain = filter.trim();
         }
         else
         {
			// domain and properties
            domain = filter.substring(0,separator).trim();
            props  = filter.substring(separator+1).trim();
		 }

         if (domain.equals(""))    domain = "*";

         if ( props.equals("")    )   props  = "*,*";
         if ( props.endsWith("," ))   props += "*";
         if (!props.endsWith(",*"))   props += ",*";
         if ( props.equals("*,*" ))   props =  "*";

         filter = domain + ":" + props;;

		 if (filter.equals("*:*"))
			 filter = "";
      }
	  else
	  {
	     filter = "";
      }

	  // update request filter and store filter in session context,
	  // so it can be used when no filter has been submitted in 
	  // current request 
      request.setAttribute(FILTER_PARAM, filter);

      if( session != null )
	  {
	     session.setAttribute(FILTER_PARAM, filter);
	  }

      try
      {
         Iterator mbeans = Server.getDomainData(filter);
         request.setAttribute("mbeans", mbeans);
         RequestDispatcher rd = this.getServletContext().getRequestDispatcher("/displayMBeans.jsp");
         rd.forward(request, response);
      }
      catch(JMException e)
      {
         throw new ServletException("Failed to get MBeans", e);
      }
   }

   /** Display an mbeans attributes and operations
    */
   private void inspectMBean(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException
   {
      String name = request.getParameter("name");
      log.trace("inspectMBean, name="+name);
      try
      {
         MBeanData data = Server.getMBeanData(name);
         request.setAttribute("mbeanData", data);
         RequestDispatcher rd = this.getServletContext().getRequestDispatcher("/inspectMBean.jsp");
         rd.forward(request, response);
      }
      catch(JMException e)
      {
         throw new ServletException("Failed to get MBean data", e);
      }
   }
   
   /** Update the writable attributes of an mbean
    */
   private void updateAttributes(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException
   {
      String name = request.getParameter("name");
      log.trace("updateAttributes, name="+name);
      Enumeration paramNames = request.getParameterNames();
      HashMap attributes = new HashMap();
      while( paramNames.hasMoreElements() )
      {
         String param = (String) paramNames.nextElement();
         if( param.equals("name") || param.equals("action") )
            continue;
         String value = request.getParameter(param);
         log.trace("name="+param+", value='"+value+"'");
         // Ignore null values, these are empty write-only fields
         if( value == null || value.length() == 0 )
            continue;
         attributes.put(param, value);
      }

      try
      {
         AttributeList newAttributes = Server.setAttributes(name, attributes);
         MBeanData data = Server.getMBeanData(name);
         request.setAttribute("mbeanData", data);
         RequestDispatcher rd = this.getServletContext().getRequestDispatcher("/inspectMBean.jsp");
         rd.forward(request, response);
      }
      catch(JMException e)
      {
         throw new ServletException("Failed to update attributes", e);
      }
   }
   /** Invoke an mbean operation given the index into the MBeanOperationInfo{}
    array of the mbean.
    */
   private void invokeOp(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException
   {
      String name = request.getParameter("name");
      log.trace("invokeOp, name="+name);
      String[] args = getArgs(request);
      String methodIndex = request.getParameter("methodIndex");
      if( methodIndex == null || methodIndex.length() == 0 )
         throw new ServletException("No methodIndex given in invokeOp form");
      int index = Integer.parseInt(methodIndex);
      try
      {
         OpResultInfo opResult = Server.invokeOp(name, index, args);
         request.setAttribute("opResultInfo", opResult);
         RequestDispatcher rd = this.getServletContext().getRequestDispatcher("/displayOpResult.jsp");
         rd.forward(request, response);
      }
      catch(JMException e)
      {
         throw new ServletException("Failed to invoke operation", e);
      }
   }

   /** Invoke an mbean operation given the method name and its signature.
    */
   private void invokeOpByName(HttpServletRequest request, HttpServletResponse response)
      throws ServletException, IOException
   {
      String name = request.getParameter("name");
      log.trace("invokeOpByName, name="+name);
      String[] argTypes = request.getParameterValues("argType");
      String[] args = getArgs(request);
      String methodName = request.getParameter("methodName");
      if( methodName == null )
         throw new ServletException("No methodName given in invokeOpByName form");
      try
      {
         OpResultInfo opResult = Server.invokeOpByName(name, methodName, argTypes, args);
         request.setAttribute("opResultInfo", opResult);
         RequestDispatcher rd = this.getServletContext().getRequestDispatcher("/displayOpResult.jsp");
         rd.forward(request, response);
      }
      catch(JMException e)
      {
         throw new ServletException("Failed to invoke operation", e);
      }
   }

   /** Extract the argN values from the request into a String[]
   */
   private String[] getArgs(HttpServletRequest request)
   {
      ArrayList argList = new ArrayList();
      for(int i = 0; true ; i++)
      {
         String name = "arg" + i;
         String value = request.getParameter(name);
         if( value == null )
            break;
         argList.add(value);
         log.trace(name+"="+value);
      }
      String[] args = new String[argList.size()];
      argList.toArray(args);
      return args;
   }
}

