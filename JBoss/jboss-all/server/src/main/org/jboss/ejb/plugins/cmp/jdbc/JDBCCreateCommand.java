/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/
package org.jboss.ejb.plugins.cmp.jdbc;

import java.lang.reflect.Method;

import javax.ejb.CreateException;

import org.jboss.deployment.DeploymentException;
import org.jboss.ejb.EntityEnterpriseContext;

/**
 * Interface to a pluggable command to create an entity
 *
 * @author <a href="mailto:jeremy@boynes.com">Jeremy Boynes</a>
 */
public interface JDBCCreateCommand
{
   void init(JDBCStoreManager manager) throws DeploymentException;

   Object execute(Method m, Object[] args, EntityEnterpriseContext ctx) throws CreateException;
}
