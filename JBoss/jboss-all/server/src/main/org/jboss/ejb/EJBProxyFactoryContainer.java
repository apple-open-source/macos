/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.ejb;

/**
 * This is an interface for Containers that uses EJBProxyFactory.
 *
 * <p>EJBProxyFactory's may communicate with the Container through
 *    this interface.
 *
 * @see EJBProxyFactory
 * 
 * @author <a href="mailto:rickard.oberg@telkel.com">Rickard Öberg</a>
 * @author <a href="mailto:docodan@mvcsoft.com">Daniel OConnor</a>
 * @version $Revision: 1.1 $
 */
public interface EJBProxyFactoryContainer
{
   /**
    * ???
    *
    * @return ???
    */
   Class getHomeClass();
   
   /**
    * ???
    *
    * @return ???
    */
   Class getRemoteClass();
   
   /**
    * ???
    *
    * @return ???
    */
   Class getLocalHomeClass();
   
   /**
    * ???
    *
    * @return ???
    */
   Class getLocalClass();
	
   /**
    * ???
    *
    * @return ???
    */
   EJBProxyFactory getProxyFactory();
   
   /**
    * ???
    *
    * @return ???
    */
   LocalProxyFactory getLocalProxyFactory();
}

