/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
*/
package org.jboss.net.sample.hello.ejb;

import javax.ejb.CreateException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;

import org.apache.log4j.Category;


/**
	* This is a single method simple SessionBean Hello World program.  The
	* xdoclet comments generate all the interfaces and JBoss.net deployment
	* descriptors.
	*
	* @ejb:bean		name="Hello"
	*						jndi-name="Hello"
	*						type="Stateless"
	*						view-type="local" 
	* @ejb:ejb-ref	ejb-name="Hello"
	*              	view-type="local"
	*              	ref-name="Hello"
	* @ejb:transaction type="Required"
	* @ejb:transaction-type type="Container"
	*
	* @jboss-net:web-service urn="Hello"
	*                        expose-all="true" 
	* @author <a href="mailto:fbrier@multideck.com">Frederick N. Brier</a>
	* @version $Revision: 1.2 $
*/
public class HelloBean implements SessionBean
{
   protected transient Category		log = Category.getInstance( getClass() );

   
	/**
		* @ejb:interface-method view-type="local"
	*/
   public String hello( String name )
   {
      return "Hello " + name;
   }

   public void ejbCreate() throws CreateException {}
   
	public void setSessionContext( SessionContext sessionContext ) {}
	
   public void ejbActivate() {}
	
   public void ejbPassivate() {}
	
   public void ejbRemove() {}
	
}   // of class HelloBean
