package org.jboss.resource.adapter.jdbc.xa.oracle;


import java.lang.reflect.Method;
import javax.management.ObjectName;
import javax.transaction.xa.XAException;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.tm.XAExceptionFormatter;
import org.jboss.logging.Logger;

/**
 * OracleXAExceptionFormatter.java
 *
 * Created: Tue Jan 28 12:45:22 2003
 *
 * @author <a href="mailto:igorfie at yahoo dot com">Igor Fedorenko</a>.
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version $Revision: 1.1.2.4 $
 *
 * @jmx.mbean extends="org.jboss.system.ServiceMBean"
 */

public class OracleXAExceptionFormatter
   extends ServiceMBeanSupport
   implements XAExceptionFormatter, OracleXAExceptionFormatterMBean
{

   private static final String EXCEPTION_CLASS_NAME = "oracle.jdbc.xa.OracleXAException";

   private static final Object[] NOARGS = {};

   private ObjectName transactionManagerService;

   private Class oracleXAExceptionClass;

   private Method getXAError;
   private Method getXAErrorMessage;
   private Method getOracleError;
   private Method getOracleSQLError;

   public OracleXAExceptionFormatter() {
      
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
      oracleXAExceptionClass = Thread.currentThread().getContextClassLoader().loadClass(EXCEPTION_CLASS_NAME);
      getXAError = oracleXAExceptionClass.getMethod("getXAError", new Class[] {});
      getXAErrorMessage = oracleXAExceptionClass.getMethod("getXAErrorMessage",
							   new Class[] {getXAError.getReturnType()});
      getOracleError = oracleXAExceptionClass.getMethod("getOracleError", new Class[] {});
      getOracleSQLError = oracleXAExceptionClass.getMethod("getOracleSQLError", new Class[] {});

      getServer().invoke(transactionManagerService,
			 "registerXAExceptionFormatter",
			 new Object[] {oracleXAExceptionClass, this},
			 new String[] {Class.class.getName(), XAExceptionFormatter.class.getName()});
   }

   protected void stopService() throws Exception
   {
      getServer().invoke(transactionManagerService,
			 "unregisterXAExceptionFormatter",
			 new Object[] {oracleXAExceptionClass},
			 new String[] {Class.class.getName()});

      oracleXAExceptionClass = null;

      getXAError = null;
      getXAErrorMessage = null;
      getOracleError = null;
      getOracleSQLError = null;
   }

   public void formatXAException(XAException xae, Logger log)
   {
      try 
      {
	 log.warn(
	    "xa error: "
	    + getXAError.invoke(xae, NOARGS)
	    + " (" + getXAErrorMessage.invoke(xae, new Object[] {getXAError.invoke(xae, NOARGS)}) + "); " 
	    + "oracle error: " + getOracleError.invoke(xae, NOARGS) + "; "
	    + "oracle sql error: " + getOracleSQLError.invoke(xae, NOARGS) + ";", xae);
      }
      catch (Exception e)
      {
	 log.info("Problem trying to format XAException: ", e); 
      } // end of try-catch
      
   }
   
}// OracleXAExceptionFormatter
