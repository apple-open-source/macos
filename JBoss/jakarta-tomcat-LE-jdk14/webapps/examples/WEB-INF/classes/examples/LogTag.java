package examples;


import javax.servlet.jsp.*;
import javax.servlet.jsp.tagext.*;

import java.io.IOException;

/**
 * Log the contents of the body. Could be used to handle errors etc. 
 */
public class LogTag 
    extends ExampleTagBase
{
    boolean toBrowser = false;
    
    public void setToBrowser(String value) {
        if (value == null)
            toBrowser = false;
        else if (value.equalsIgnoreCase("true"))
            toBrowser = true;
        else
            toBrowser = false;
    }

    public int doStartTag() throws JspException {
        return EVAL_BODY_TAG;
    }
    
    public int doAfterBody() throws JspException {
        try {
            String s = bodyOut.getString();
            System.err.println(s);
            if (toBrowser)
                bodyOut.writeOut(bodyOut.getEnclosingWriter());
            return SKIP_BODY;
        } catch (IOException ex) {
            throw new JspTagException(ex.toString());
        }
    }
}

    
        
    
