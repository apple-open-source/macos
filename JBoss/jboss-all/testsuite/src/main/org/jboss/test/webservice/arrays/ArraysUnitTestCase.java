/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

// $Id: ArraysUnitTestCase.java,v 1.1.2.1 2003/09/29 23:46:49 starksm Exp $

package org.jboss.test.webservice.arrays;

import java.net.URL;

import junit.framework.Test;

import org.jboss.test.webservice.AxisTestCase;
import org.jboss.test.webservice.arrays.Arrays;

/**
 * Tests remote accessibility of stateless ejb bean
 * @created 5. Oktober 2001, 12:11
 * @author <a href="mailto:Christoph.Jung@infor.de">Christoph G. Jung</a>
 * @version $Revision: 1.1.2.1 $
 */

public class ArraysUnitTestCase extends AxisTestCase
{


   protected String ARRAYS_END_POINT = END_POINT + "/Arrays";

   // Constructors --------------------------------------------------
   public ArraysUnitTestCase(String name)
   {
      super(name);
   }

   /** the session bean with which we interact */
   Arrays arrays;

   /** setup the bean */
   public void setUp() throws Exception
   {
      super.setUp();
      arrays = (Arrays) createAxisService(Arrays.class,
         new URL(ARRAYS_END_POINT));
   }

   /** where the config is stored */
   protected String getAxisConfiguration()
   {
      return "webservice/arrays/client/" + super.getAxisConfiguration();
   }

   public void testArrays() throws Exception
   {
      Object[] values = new Object[]{new String("Test"), new Integer(1), new ArraysData()};
      assertTrue("Return value must be same.", java.util.Arrays.equals(arrays.arrays(values), values));
   }

   public void testReverse() throws Exception
   {
      Object[] values = new Object[]{new String("Test"), new Integer(1), new ArraysData()};
      Object[] expected = new Object[]{new ArraysData(), new Integer(1), new String("Test")};
      assertTrue("Return value must be reversed.", java.util.Arrays.equals(arrays.reverse(values), expected));
   }

   /** this is to deploy the whole ear */
   public static Test suite() throws Exception
   {
      return getDeploySetup(ArraysUnitTestCase.class, "arrays.ear");
   }

   public static void main(String[] args)
   {
      junit.textui.TestRunner.run(ArraysUnitTestCase.class);
   }

}
