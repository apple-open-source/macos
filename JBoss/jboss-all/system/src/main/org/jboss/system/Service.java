/***************************************
 *                                     *
 *  JBoss: The OpenSource J2EE WebOS   *
 *                                     *
 *  Distributable under LGPL license.  *
 *  See terms of license at gnu.org.   *
 *                                     *
 ***************************************/

package org.jboss.system;

/**
 * The Service interface.
 *      
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>.
 * @version $Revision: 1.1 $
 *
 * <p><b>20010830 marc fleury:</b>
 * <ul>
 *   <li>Initial import
 * </ul>
 * <p><b>20011111 david jencks:</b>
 * <ul>
 *   <li>removed init and destroy methods
 * </ul>
 *  <p><b>20011208 marc fleury:</b>
 * <ul>
 *   <li>init and destroy were put back by david
 *   <li>init becomes create
 * </ul>
 */
public interface Service
{
   /**
    * create the service, do expensive operations etc 
    */
   void create() throws Exception;
   
   /**
    * start the service, create is already called
    */
   void start() throws Exception;
   
   /**
    * stop the service
    */
   void stop();
   
   /**
    * destroy the service, tear down 
    */
   void destroy();
}
