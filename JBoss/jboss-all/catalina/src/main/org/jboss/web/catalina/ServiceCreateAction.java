package org.jboss.web.catalina;

import java.util.Stack;

import org.apache.catalina.util.xml.SaxContext;
import org.apache.catalina.util.xml.XmlAction;

/** This action creates an EmbeddedService and adds it to the EmbeddedCatalina

@author Scott.Stark@jboss.org
@version $Revision: 1.2 $
 */
public class ServiceCreateAction extends XmlAction
{

   /** The top of the stack must be an EmbeddedCatalina object
    */
   public void start(SaxContext ctx) throws Exception
   {
      Stack st = ctx.getObjectStack();
      int top = ctx.getTagCount()-1;
      String tag = ctx.getTag(top);
      EmbeddedCatalina catalina = (EmbeddedCatalina) st.peek();

      EmbeddedService service = new EmbeddedService();
      service.setServer(catalina);
      st.push(service);
      if( ctx.getDebug() > 0 ) ctx.log("new "  + EmbeddedService.class + " "  + tag  + " " + service);
   }

   public void cleanup(SaxContext ctx)
   {
      Stack st = ctx.getObjectStack();
      String tag = ctx.getTag(ctx.getTagCount()-1);
      Object o = st.pop();
      if( ctx.getDebug() > 0 )
         ctx.log("pop " + tag + " " + o.getClass().getName() + ": " + o);
   }
}
