package org.jboss.web.tomcat.tc4;

import java.util.Stack;

import org.apache.catalina.Engine;
import org.apache.catalina.startup.Embedded;
import org.apache.commons.digester.Digester;
import org.apache.commons.digester.Rule;

/** This action adds the current Engine on the top of the stack to the
 Embedded object on the stack.

 TOMCAT 4.1.12 UPDATE: Extends org.apache.jakarta.commons.Rule

@author Scott.Stark@jboss.org
@version $Revision: 1.1.1.1 $
 */
public class AddEngineAction extends Rule
{   
   public AddEngineAction()
   {
   }

   /** The stack must look like Embedded/Service/Engine
    */
   public void end() throws Exception
   {
      Engine engine = (Engine) digester.pop();
      Object service = digester.pop();
      Embedded catalina = (Embedded) digester.peek();
      digester.push(service);
      digester.push(engine);
      catalina.addEngine(engine);
   }
}
