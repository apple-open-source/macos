
/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 *
 */

package org.jboss.test.proxycompiler.test; // Generated package name

import junit.framework.*;

import org.jboss.test.JBossTestCase;

import org.jboss.test.proxycompiler.Util;
import org.jboss.test.proxycompiler.beans.interfaces.ProxyCompilerTest;
import org.jboss.test.proxycompiler.beans.interfaces.ProxyCompilerTestHome;

import java.util.Iterator;
import java.util.Collection;

/**
 * Tests the proxy generation for beans.
 * 
 * <p>Currently only tests CMP2 beans.
 *
 * Created: Wed Jan 30 00:16:57 2002
 *
 * @version <tt>$Revision: 1.1 $</tt>
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 */
public class BeanProxyUnitTestCase 
   extends JBossTestCase 
{
   private ProxyCompilerTestHome home;
   
   public BeanProxyUnitTestCase(final String name)
   {
      super(name);
   }

   public static Test suite() throws Exception
   {
      return getDeploySetup(ProxyCompilerUnitTestCase.class, "proxycompiler-test.jar");
   }

   public void testProxyCompilerTest() throws Exception
   {
      
      Integer pk = new Integer(1);
      ProxyCompilerTest bean = home.create(pk);
      
      assertEquals("Object argument error", pk, bean.getPk());
      
      bean.setBool(true);
      assertTrue("boolean argument error", bean.getBool());
      
      byte byteArg = (byte)123;
      bean.setByte(byteArg);
      assertEquals("byte argument error", byteArg, bean.getByte());
      
      char charArg = 'N';
      bean.setChar(charArg);
      assertEquals("char argument error", charArg, bean.getChar());
      
      double doubleArg = 1.5;
      bean.setDouble(doubleArg);
      assertEquals("double argument error", doubleArg, bean.getDouble(), 0.01);

      float floatArg = 1.5f;
      bean.setFloat(floatArg);
      assertEquals("float argument error", floatArg, bean.getFloat(), 0.01f);
      
      int intArg = 234;
      bean.setInt(intArg);
      assertEquals("int argument error", intArg, bean.getInt());
      
      long longArg = 23456L;
      bean.setLong(longArg);
      assertEquals("long argument error", longArg, bean.getLong());
      
      short shortArg = (short)7;
      bean.setShort(shortArg);
      assertEquals("short argument error", shortArg, bean.getShort());
      
      Object[] objectArrayArg = new Object[]{new Integer(4), "Hello Mum", new Float(123.0)};
      bean.setObjectArray(objectArrayArg);
      Object[] objectReturnArg = bean.getObjectArray();
      for ( int i = 0;  i < objectArrayArg.length;  i++ ) {
         assertEquals("Object[] argument error", objectArrayArg[i], objectReturnArg[i]);
      }
      
      int[] intArrayArg = new int[]{2, 4, 6, 8};
      bean.setIntArray(intArrayArg);
      int[] intReturnArg = bean.getIntArray();
      for ( int i = 0;  i < intArrayArg.length;  i++ ) {
         assertEquals("int[] argument error", intArrayArg[i], intReturnArg[i]);
      }

      assertTrue("noArgs argument error", bean.noArgsMethod());
      
      String stringRep = Util.getStringRepresentation(intArg, pk, intArrayArg, objectArrayArg);

      String returnArg = bean.complexSignatureMethod(intArg, pk, intArrayArg, objectArrayArg);

      assertEquals("complex argument error", stringRep, returnArg);

   }

   protected void setUp()
      throws Exception
   {
      home = (ProxyCompilerTestHome)getInitialContext().lookup("ProxyCompilerTest");
      getLog().debug("Remove ProxyCompilerTest bean instances");

      Collection beansColl = home.findAll();
      
      for(Iterator beans = beansColl.iterator(); beans.hasNext();) {
         ProxyCompilerTest bean = (ProxyCompilerTest)beans.next();
         getLog().debug("Removing " + bean.getPrimaryKey());
         bean.remove();
      }
   }

}// ProxyCompilerTestUnitTestCase
