/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.mx.service;

import javax.management.ObjectName;

/**
 * Defines constants for JMX services.
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.3.8.2 $
 *   
 */
public interface ServiceConstants
{
   /** 
    * The object name domain <tt>'JBossMXImplementation'<tt> can be used by
    * JBossMX service implementations.
    */
   final static String JBOSSMX_DOMAIN           = "JBossMXImplementation";
   
   /** Default object name for persistence interceptor with <tt>ON_TIMER</tt> policy. */
   final static String PERSISTENCE_TIMER        = new String(JBOSSMX_DOMAIN + ":name=PersistenceTimer");
   
   /** DTD file name for XMLMBeanLoader, version 1.0 */
   final static String MBEAN_LOADER_DTD_1_0     = "JBossMX_MBeanLoader_1_0.dtd";

   /** DTD file name for JBossMX XMBean, version 1.0 */
   final static String JBOSSMX_XMBEAN_DTD_1_0   = "jboss_xmbean_1_0.dtd";
   final static String PUBLIC_JBOSSMX_XMBEAN_DTD_1_0   = "-//JBoss//DTD JBOSS XMBEAN 1.0//EN";
   
   /** DTD file name for JBossMX XMBean, version 1.1 */
   final static String JBOSSMX_XMBEAN_DTD_1_1   = "jboss_xmbean_1_1.dtd";
   final static String PUBLIC_JBOSSMX_XMBEAN_DTD_1_1   = "-//JBoss//DTD JBOSS XMBEAN 1.1//EN";
   
   /** The original, book version of the XMBean document definition */
   final static String XMBEAN_DTD               = "xmbean.dtd";
   
}
      



