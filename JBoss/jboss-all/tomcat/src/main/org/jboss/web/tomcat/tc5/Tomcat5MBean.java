/*
 * JBoss, the OpenSource WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.web.tomcat.tc5;

import org.jboss.web.AbstractWebContainerMBean;

/** 
 * An implementation of the AbstractWebContainer for the Jakarta Tomcat5
 * servlet container. It has no code dependency on tomcat - only the new JMX
 * model is used.
 * 
 * Tomcat5 is organized as a set of mbeans - just like jboss.
 * 
 * @see AbstractWebContainer
 * 
 * @author Scott.Stark@jboss.org
 * @authro Costin Manolache
 * @version $Revision: 1.1.1.1 $
 */
public interface Tomcat5MBean extends AbstractWebContainerMBean
{

    public String getDomain();

    /** The most important atteribute - defines the managed domain.
     *  A catalina instance (engine) corresponds to a JMX domain, that's
     *  how we know where to deploy webapps.
     *
     * @param catalinaHome
     */
    public void setDomain(String catalinaHome);

}
