/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.jbossmx.compliance.standard.support;

public interface StandardDerived2MBean
{
   void setDerivedValue(String derived);
   String getParentValue();
}
