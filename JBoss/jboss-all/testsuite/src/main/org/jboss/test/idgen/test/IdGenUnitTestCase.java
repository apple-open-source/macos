/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
/*
 * Copyright 1999 by dreamBean Software,
 * All rights reserved.
 */
package org.jboss.test.idgen.test;
import java.lang.reflect.*;

import java.util.*;
import javax.ejb.*;
import javax.naming.*;

import junit.framework.Test;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.jboss.test.JBossTestCase;

import org.jboss.test.idgen.interfaces.*;

/**
 * @see       <related>
 * @author    Author: d_jencks only added JBossTestCase and logging
 * @version   $Revision: 1.5 $
 */
public class IdGenUnitTestCase
       extends JBossTestCase
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------


   // Constructors --------------------------------------------------
   /**
    * Constructor for the IdGenUnitTestCase object
    *
    * @param name  Description of Parameter
    */
   public IdGenUnitTestCase(String name)
   {
      super(name);
   }

   // Public --------------------------------------------------------
   /**
    * A unit test for JUnit
    *
    * @exception Exception  Description of Exception
    */
   public void testGenerator()
          throws Exception
   {
      IdGeneratorHome home = (IdGeneratorHome)getInitialContext().lookup(IdGeneratorHome.JNDI_NAME);
      IdGenerator generator = home.create();

      generator.getNewId("Account");
      generator.getNewId("Account");
      generator.getNewId("Account");

      generator.getNewId("Customer");
      generator.getNewId("Customer");
      generator.getNewId("Customer");

      generator.remove();
   }

   /**
    * The JUnit setup method
    *
    * @exception Exception  Description of Exception
    */
   protected void setUp()
          throws Exception
   {
      getLog().debug("Remove id counters");
      {
         IdCounterHome home = (IdCounterHome)new InitialContext().lookup(IdCounterHome.JNDI_NAME);
         Collection counters = home.findAll();
         Iterator enum = counters.iterator();
         while (enum.hasNext())
         {
            EJBObject obj = (EJBObject)enum.next();
            getLog().debug("Removing " + obj.getPrimaryKey());
            obj.remove();
         }
      }
   }

   public static Test suite() throws Exception
   {
      return getDeploySetup(IdGenUnitTestCase.class, "idgen.jar");
   }

}
