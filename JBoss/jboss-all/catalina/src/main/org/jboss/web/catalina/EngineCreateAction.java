package org.jboss.web.catalina;

import java.util.Stack;

import org.xml.sax.AttributeList;

import org.apache.catalina.Engine;
import org.apache.catalina.startup.Embedded;
import org.apache.catalina.util.xml.SaxContext;
import org.apache.catalina.util.xml.XmlAction;

/** This action creates an Engine and sets the Realm and Logger from the
 Embedded object on the stack.

@author Scott.Stark@jboss.org
@version $Revision: 1.2 $
 */
public class EngineCreateAction extends XmlAction
{
   String className;
   String attrName;
   
   /**
    * Create an object of the specified class name.
    *
    * @param classN Fully qualified name of the Java class to instantiate
    */
   public EngineCreateAction(String className)
   {
      this.className = className;
   }
   
   /**
    * Create an object of the specified default class name, unless an
    * attribute with the specified name is present, in which case the value
    * of this attribute overrides the default class name.
    *
    * @param classN Fully qualified name of the Java class to instantiate
    *  if the specified attribute name is not present
    * @param attrib Name of the attribute that may contain a fully qualified
    *  name of a Java class that overrides the default
    */
   public EngineCreateAction(String className, String attrName)
   {
      this.className = className;
      this.attrName = attrName;
   }

   /** The stack must look like Embedded/Service
    */
   public void start(SaxContext ctx) throws Exception
   {
      Stack st = ctx.getObjectStack();
      int top = ctx.getTagCount()-1;
      String tag = ctx.getTag(top);
      String classN = className;
      Object service = st.pop();
      Embedded catalina = (Embedded) st.peek();
      st.push(service);

      if( attrName != null )
      {
         AttributeList attributes = ctx.getAttributeList( top );
         if (attributes.getValue(attrName) != null)
            classN = attributes.getValue(attrName);
      }
      Class c = Class.forName( classN );
      Engine e = (Engine) c.newInstance();
      e.setLogger(catalina.getLogger());
      e.setRealm(catalina.getRealm());
      st.push(e);
      if( ctx.getDebug() > 0 ) ctx.log("new "  + attrName + " " + classN + " "  + tag  + " " + e);
   }

   public void cleanup( SaxContext ctx)
   {
      Stack st = ctx.getObjectStack();
      String tag = ctx.getTag(ctx.getTagCount()-1);
      Object o = st.pop();
      if( ctx.getDebug() > 0 ) ctx.log("pop " + tag + " " + o.getClass().getName() + ": " + o);
   }
   
}
