package examples;


import javax.servlet.*;
import javax.servlet.jsp.*;
import javax.servlet.jsp.tagext.*;

import java.io.*;

/**
 * Display the sources of the JSP file.
 */
public class ShowSource
    extends TagSupport
{
    String jspFile;
    
    public void setJspFile(String jspFile) {
        this.jspFile = jspFile;
    }

    public int doEndTag() throws JspException {
	if ((jspFile.indexOf( ".." ) >= 0) ||
            (jspFile.toUpperCase().indexOf("/WEB-INF/") != 0) ||
            (jspFile.toUpperCase().indexOf("/META-INF/") != 0))
	    throw new JspTagException("Invalid JSP file " + jspFile);

        InputStream in
            = pageContext.getServletContext().getResourceAsStream(jspFile);

        if (in == null)
            throw new JspTagException("Unable to find JSP file: "+jspFile);

        InputStreamReader reader = new InputStreamReader(in);
	JspWriter out = pageContext.getOut();


        try {
            out.println("<body>");
            out.println("<pre>");
            for(int ch = in.read(); ch != -1; ch = in.read())
                if (ch == '<')
                    out.print("&lt;");
                else
                    out.print((char) ch);
            out.println("</pre>");
            out.println("</body>");
        } catch (IOException ex) {
            throw new JspTagException("IOException: "+ex.toString());
        }
        return super.doEndTag();
    }
}

    
        
    
