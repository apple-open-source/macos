/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.helloiiop.ejb;

import javax.ejb.EJBException;

import org.jboss.test.util.ejb.SessionSupport;
import org.jboss.test.helloiiop.interfaces.Hello;
import org.jboss.test.helloiiop.interfaces.HelloData;

/**
 *      
 *   @author Scott.Stark@jboss.org
 *   @version $Revision: 1.1 $
 */
public class HelloBean
   extends SessionSupport
{
   public String hello(String name)
   {
      return "Hello "+name+"!";
   }

   public Hello helloHello(Hello hello)
   {
      return hello;
   }

   public String howdy(HelloData name)
   {
      return "Howdy "+name.getName()+"!";
   }

   public void throwException()
   {
      throw new EJBException("Something went wrong");
   }
}
