package org.jboss.web.tomcat.tc4;

import java.util.Stack;

import org.xml.sax.AttributeList;
import org.xml.sax.Attributes;

import org.apache.catalina.Engine;
import org.apache.catalina.startup.Embedded;
import org.apache.commons.digester.Rule;
import org.apache.commons.digester.Digester;

/** This action creates an Engine and sets the Realm and Logger from the
 Embedded object on the stack.

 TOMCAT 4.1.12 UPDATE: Extends org.apache.jakarta.commons.Rule

@author Scott.Stark@jboss.org
@version $Revision: 1.1.1.1 $
 */
public class EngineCreateAction extends Rule
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

    public void begin(Attributes attributes) throws Exception {
        String classN = className;
        Object service = digester.pop();
        Embedded catalina = (Embedded) digester.peek();
        digester.push(service);
        if( attrName != null ) {
           if (attributes.getValue(attrName) != null)
              classN = attributes.getValue(attrName);
        }
        Class c = Class.forName( classN );
        Engine e = (Engine) c.newInstance();
        e.setLogger(catalina.getLogger());
        e.setRealm(catalina.getRealm());
        digester.push(e);
    }

    public void end() throws Exception {
        digester.pop();
    }
}
