package org.jboss.resource.connectionmanager;


import javax.management.ObjectName;
import javax.transaction.xa.XAException;

import org.jboss.logging.Logger;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.tm.XAExceptionFormatter;

/**
 * JBossLocalXAExceptionFormatter.java
 *
 * Created: Tue Jan 28 12:45:22 2003
 *
 * @author <a href="mailto:igorfie at yahoo dot com">Igor Fedorenko</a>.
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version
 *
 * @jmx.mbean
 */

public class JBossLocalXAExceptionFormatter
   extends ServiceMBeanSupport
   implements XAExceptionFormatter, JBossLocalXAExceptionFormatterMBean
{

   private static final String EXCEPTION_CLASS_NAME = "org.jboss.resource.connectionmanager.JBossLocalXAException";

   private static final Object[] NOARGS = {};

   private ObjectName transactionManagerService;


   public JBossLocalXAExceptionFormatter() {
      
   }

   
   
   /**
    * mbean get-set pair for field transactionManagerService
    * Get the value of transactionManagerService
    * @return value of transactionManagerService
    *
    * @jmx:managed-attribute
    */
   public ObjectName getTransactionManagerService()
   {
      return transactionManagerService;
   }

   /**
    * The <code>setTransactionManagerService</code> method 
    *
    * @param transactionManagerService an <code>ObjectName</code> value
    *
    * @jmx.managed-attribute
    */
   public void setTransactionManagerService(ObjectName transactionManagerService)
   {
      this.transactionManagerService = transactionManagerService;
   }

   protected void startService() throws Exception
   {

      getServer().invoke(transactionManagerService,
			 "registerXAExceptionFormatter",
			 new Object[] {JBossLocalXAException.class, this},
			 new String[] {Class.class.getName(), XAExceptionFormatter.class.getName()});
   }

   protected void stopService() throws Exception
   {
      getServer().invoke(transactionManagerService,
			 "unregisterXAExceptionFormatter",
			 new Object[] {JBossLocalXAException.class},
			 new String[] {Class.class.getName()});

   }

   public void formatXAException(XAException xae, Logger log)
   {
      try 
      {
	 log.warn("JBoss Local XA wrapper error: ", ((JBossLocalXAException)xae).getCause());
      }
      catch (Exception e)
      {
	 log.info("Problem trying to format XAException: ", e); 
      } // end of try-catch
      
   }
   
}// JBossLocalXAExceptionFormatter
