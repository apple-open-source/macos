package org.jboss.web.tomcat.tc4;

import java.util.Stack;

import org.apache.commons.digester.Digester;
import org.apache.commons.digester.Rule;
import org.xml.sax.Attributes;
import org.jboss.web.tomcat.tc4.EmbeddedCatalina;
import org.jboss.web.tomcat.tc4.EmbeddedService;

/** This action creates an EmbeddedService and adds it to the EmbeddedCatalina

 TOMCAT 4.1.12 UPDATE: Extends org.apache.jakarta.commons.Rule

@author Scott.Stark@jboss.org
@version $Revision: 1.1.1.1 $
 */
public class ServiceCreateAction extends Rule
{
   /** The top of the stack must be an EmbeddedCatalina object
    */

    public void begin(Attributes attributes) throws Exception {
        EmbeddedCatalina catalina = (EmbeddedCatalina) digester.peek();
        EmbeddedService service = new EmbeddedService();
        service.setEmbeddedServer(catalina);
        digester.push(service);
    }

    public void end() throws Exception {
        digester.pop();
    }
}
