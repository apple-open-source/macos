/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.webservice.arrays.server;

import org.jboss.test.util.ejb.SessionSupport;

/**
 * The typical Hello Session Bean this time
 * as a web-service.
 * @author jung
 * @version $Revision: 1.1.2.1 $
 * @ejb:bean name="Arrays"
 *           display-name="Arrays World Bean"
 *           type="Stateless"
 *           view-type="remote"
 *           jndi-name="arrays/Arrays"
 * @ejb:interface remote-class="org.jboss.test.webservice.arrays.Arrays" extends="javax.ejb.EJBObject"
 * @ejb:home remote-class="org.jboss.test.webservice.arrays.ArraysHome" extends="javax.ejb.EJBHome"
 * @ejb:transaction type="Required"
 * @jboss-net:web-service urn="Arrays"
 */

public class ArraysBean
   extends SessionSupport implements javax.ejb.SessionBean
{
   /**
    * @jboss-net:web-method
    * @ejb:interface-method view-type="remote"
    */
   public Object[] arrays(Object[] values)
   {
      return values;
   }

   /**
    * @jboss-net:web-method
    * @ejb:interface-method view-type="remote"
    */
   public Object[] reverse(Object[] values)
   {
      Object[] result = new Object[values.length];
      for (int i = 0; i < values.length; ++i)
         result[i] = values[values.length - i - 1];
      return result;
   }
}
