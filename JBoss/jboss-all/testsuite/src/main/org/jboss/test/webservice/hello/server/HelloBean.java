/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.webservice.hello.server;

import javax.ejb.EJBException;

import org.jboss.test.util.ejb.SessionSupport;

import org.jboss.test.webservice.hello.HelloData;
import org.jboss.test.webservice.hello.Hello;

/**
 * The typical Hello Session Bean this time
 * as a web-service.
 * @author jung
 * @version $Revision: 1.1.2.1 $
 * @ejb:bean name="Hello"
 *           display-name="Hello World Bean"
 *           type="Stateless"
 *           view-type="remote"
 *           jndi-name="hello/Hello"
 * @ejb:interface remote-class="org.jboss.test.webservice.hello.Hello" extends="javax.ejb.EJBObject"
 * @ejb:home remote-class="org.jboss.test.webservice.hello.HelloHome" extends="javax.ejb.EJBHome"
 * @ejb:transaction type="Required"
 * @jboss-net:web-service urn="Hello"
 */

public class HelloBean
   extends SessionSupport implements javax.ejb.SessionBean
{
   /**
    * @jboss-net:web-method
    * @ejb:interface-method view-type="remote"
    */

   public String hello(String name)
   {
      return "Hello " + name + "!";
   }

   /*
    * @ejb:interface-method view-type="remote"
   public Hello helloHello(Hello hello)
   {
      return hello;
   }*/

   /**
    * @jboss-net:web-method
    * @ejb:interface-method view-type="remote"
    */

   public String howdy(HelloData name)
   {
      return "Howdy " + name.getName() + "!";
   }

   public void throwException()
   {
      throw new EJBException("Something went wrong");
   }
}
