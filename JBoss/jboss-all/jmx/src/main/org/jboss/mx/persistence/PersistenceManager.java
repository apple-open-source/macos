/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.persistence;

import javax.management.MBeanException;
import javax.management.MBeanInfo;
import org.jboss.mx.modelmbean.ModelMBeanInvoker;

/**
 * Persistence manager interface adds <tt>MBeanInfo</tt> to <tt>PersistenMBean</tt>
 * operations. This allows generic persistence manager implementations to store
 * and load the metadata of an MBean.
 *
 * @see javax.management.PersistentMBean
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @author Matt Munz
 * @version $Revision: 1.1.8.1 $
 *   
 */
public interface PersistenceManager
{
   void load(ModelMBeanInvoker mbean, MBeanInfo metadata) throws MBeanException;
   void store(MBeanInfo metadata) throws MBeanException;
}
