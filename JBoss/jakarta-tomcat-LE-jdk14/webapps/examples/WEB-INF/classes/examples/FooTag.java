package examples;

import javax.servlet.jsp.*;
import javax.servlet.jsp.tagext.*;
import java.util.Hashtable;
import java.io.Writer;
import java.io.IOException;

/**
 * Example1: the simplest tag
 * Collect attributes and call into some actions
 *
 * <foo att1="..." att2="...." att3="...." />
 */

public class FooTag 
    extends ExampleTagBase 
{
    private String atts[] = new String[3];
    int i = 0;
    
    private final void setAtt(int index, String value) {
        atts[index] = value;
    }
    
    public void setAtt1(String value) {
        setAtt(0, value);
    }
    
    public void setAtt2(String value) {
        setAtt(1, value);
    }

    public void setAtt3(String value) {
        setAtt(2, value);
    }
    
    /**
     * Process start tag
     *
     * @return EVAL_BODY_INCLUDE
     */
    public int doStartTag() throws JspException {
        i = 0;
	return EVAL_BODY_TAG;
    }

    public void doInitBody() throws JspException {
        pageContext.setAttribute("member", atts[i]);
        i++;
    }
    
    public int doAfterBody() throws JspException {
        try {
            if (i == 3) {
                bodyOut.writeOut(bodyOut.getEnclosingWriter());
                return SKIP_BODY;
            } else
                pageContext.setAttribute("member", atts[i]);
            i++;
            return EVAL_BODY_TAG;
        } catch (IOException ex) {
            throw new JspTagException(ex.toString());
        }
    }
}

