package org.jboss.test.bench.servlet;

import java.util.Hashtable;
import java.util.Enumeration;

import javax.servlet.ServletException;

import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;

public class Dispatcher extends HttpServlet {

       static org.apache.log4j.Category log =
       org.apache.log4j.Category.getInstance(Dispatcher.class);

	public static String[] params = {"hw", "os", "ram", "cpu", "jdk", "ejb", "web", "servlet" };

	protected void doGet(HttpServletRequest req, HttpServletResponse resp)
		throws ServletException, java.io.IOException {
		try {

		resp.setHeader("Location", req.getContextPath() + "/");

		if (req.getParameter("gototest") != null)
			// save config and go to tests
			saveInfo(req, resp);
		
		else if (req.getParameter("goejb") != null)
			// test ejb
			testEjb(req, resp);

		else if (req.getParameter("goall") != null)
			// test the whole stack
			testAll(req, resp);
		
		else 
			// should not get there, go back to the main page
			req.getRequestDispatcher("/index.jsp").include(req, resp);
		} catch (Throwable t) { 
			log.debug("failed", t);
		}

	}

    /** 
	 * Saves the info from the request in the session object		
	 */
	void saveInfo(HttpServletRequest req, HttpServletResponse resp)
		throws ServletException, java.io.IOException {
		
		HttpSession session = req.getSession();
		ConfigData conf = (ConfigData)session.getAttribute("conf");

		for (int i=0; i<conf.size(); i++) {
			conf.setInfo(conf.getName(i), req.getParameter(conf.getName(i)));
		}
		
		req.getRequestDispatcher("/tests.jsp").include(req, resp);
		
	}		
	
    void testEjb(HttpServletRequest req, HttpServletResponse resp)
		throws ServletException, java.io.IOException {

		EJBTester ejbTester = new EJBTester(req);
		
		// do the test
	    ejbTester.test();
		
		req.setAttribute("ejbTester", ejbTester);

		// finally include to the correct jsp page

		req.getRequestDispatcher("/ejbResult.jsp").include(req, resp);
		
	}

	void testAll(HttpServletRequest req, HttpServletResponse resp)
		throws ServletException, java.io.IOException {

		FullTester fullTester = new FullTester(req);
		
		// do the test
	    fullTester.test();
		
		req.setAttribute("fullTester", fullTester);

		// finally include to the correct jsp page
		req.getRequestDispatcher("/allResult.jsp").include(req, resp);

	}

}


