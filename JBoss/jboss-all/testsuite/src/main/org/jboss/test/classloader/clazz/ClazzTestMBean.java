/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.classloader.clazz;

import org.jboss.system.ServiceMBean;

/**
 * A simple service for testing class loading
 * 
 * @author claudio.vesco@previnet.it
 */
public interface ClazzTestMBean extends ServiceMBean
{
   void loadClass(String clazz) throws Exception;
   
   void loadClassFromTCL(String clazz) throws Exception;
}
