package org.jboss.web.catalina;

import java.util.Stack;

import org.apache.catalina.Engine;
import org.apache.catalina.startup.Embedded;
import org.apache.catalina.util.xml.SaxContext;
import org.apache.catalina.util.xml.XmlAction;

/** This action adds the current Engine on the top of the stack to the
 Embedded object on the stack.

@author Scott.Stark@jboss.org
@version $Revision: 1.2 $
 */
public class AddEngineAction extends XmlAction
{   
   public AddEngineAction()
   {
   }

   /** The stack must look like Embedded/Service/Engine
    */
   public void end(SaxContext ctx) throws Exception
   {
      Stack st = ctx.getObjectStack();
      int top = ctx.getTagCount()-1;
      String tag = ctx.getTag(top);
      Engine engine = (Engine) st.pop();
      Object service = st.pop();
      Embedded catalina = (Embedded) st.peek();
      st.push(service);      
      st.push(engine);
      catalina.addEngine(engine);
      if( ctx.getDebug() > 0 )
         ctx.log("addEngine: "+engine.getClass()+" to Embedded");
   }   
}
