/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.varia.scheduler;

import java.security.InvalidParameterException;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Iterator;
import java.util.StringTokenizer;

import javax.management.JMException;
import javax.management.MBeanServer;
import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import javax.naming.InitialContext;
import javax.sql.DataSource;

/**
 * This Provider get its Scheduler from a Database and then adds
 * all the Schedules to the Schedule Manager.
 * The "SQL Statement" must deliver the following attributes:
 * Index  Content  Data Type
 * ----------------------------------
 *    1., Target, String
 *    2., Method_Name, String
 *    3., Method_Signature, String
 *    4., Start_Date, String
 *    5., Period, long
 *    6., Repetitions, int
 *    7., Date_Format, String
 * ATTENTION: The "Target" is Object Name of the target MBean as
 * String, the "Method_Signature" is a list of attributes separated
 * by colons which can contain:
 * <ul>
 * <li>NOTIFICATION which will be replaced by the timers notification instance
 *     (javax.management.Notification)</li>
 * <li>DATE which will be replaced by the date of the notification call
 *     (java.util.Date)</li>
 * <li>REPETITIONS which will be replaced by the number of remaining repetitions
 *     (long)</li>
 * <li>SCHEDULER_NAME which will be replaced by the Object Name of the Scheduler
 *     (javax.management.ObjectName)</li>
 * <li>any full qualified Class name which the Scheduler will be set a "null" value
 *     for it</li>
 * </ul>
 * The "Period" is an long value greater than 0.
 * The "Repetitions" can be set to "-1" which means unlimited repetitions.
 * The "Date_Format" can be null or blank to signify locale usage
 *
 * @jmx:mbean name="jboss:service=DBScheduleProvider"
 *            extends="org.jboss.varia.scheduler.AbstractScheduleProviderMBean"
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.3.2.2 $
 */
public class DBScheduleProvider
      extends AbstractScheduleProvider
      implements DBScheduleProviderMBean
{

   // -------------------------------------------------------------------------
   // Constants
   // -------------------------------------------------------------------------

   // -------------------------------------------------------------------------
   // Members
   // -------------------------------------------------------------------------

   private String mDataSourceName;
   private String mSQLStatement;

   /** The ID of the Schedule used later to remove it later **/
   private ArrayList mIDList = new ArrayList();

   // -------------------------------------------------------------------------
   // Constructors
   // -------------------------------------------------------------------------

   /**
    * Default (no-args) Constructor
    **/
   public DBScheduleProvider()
   {
   }

   // -------------------------------------------------------------------------
   // SchedulerMBean Methods
   // -------------------------------------------------------------------------

   /**
    * @return JNDI name of the Data Source used
    *
    * @jmx:managed-operation
    **/
   public String getDataSourceName()
   {
      return mDataSourceName;
   }

   /**
    * Sets the JNDI name of the Data Source. You have
    * to ensure that the DataSource is available when
    * this service is started.
    *
    * @jmx:managed-operation
    **/
   public void setDataSourceName(String pDataSourceName)
   {
      mDataSourceName = pDataSourceName;
   }

   /**
    * @return SQL Statement used to access the DB
    *
    * @jmx:managed-operation
    **/
   public String getSQLStatement()
   {
      return mSQLStatement;
   }

   /**
    * Sets the SQL Statement used to retrieve the data
    * from the Database
    *
    * @jmx:managed-operation
    **/
   public void setSQLStatement(String pSQLStatement)
   {
      mSQLStatement = pSQLStatement;
   }

   /**
    * Add the Schedule to the Schedule Manager
    *
    * @jmx:managed-operation
    **/
   public void startProviding()
         throws Exception
   {
      Connection lConnection = null;
      PreparedStatement lStatement = null;
      try
      {
         Object lTemp = new InitialContext().lookup(mDataSourceName);
         DataSource lDB = (DataSource) lTemp;
         lConnection = lDB.getConnection();
         lStatement = lConnection.prepareStatement(mSQLStatement);
         ResultSet lResult = lStatement.executeQuery();
         while (lResult.next())
         {
            int lID = addSchedule(
                  new ObjectName(lResult.getString(1)),
                  lResult.getString(2),
                  getSignature(lResult.getString(3)),
                  getStartDate(lResult.getString(4), lResult.getString(7)),
                  lResult.getLong(5),
                  lResult.getInt(6)
            );
            mIDList.add(new Integer(lID));
         }
      }
      finally
      {
         if (lStatement != null)
         {
            try
            {
               lStatement.close();
            }
            catch (Exception e)
            {
            }
         }
         if (lConnection != null)
         {
            try
            {
               lConnection.close();
            }
            catch (Exception e)
            {
            }
         }
      }
   }

   /**
    * Stops the Provider from providing causing
    * the provider to remove the Schedule
    *
    * @jmx:managed-operation
    */
   public void stopProviding()
   {
      Iterator i = mIDList.iterator();
      while (i.hasNext())
      {
         Integer lID = (Integer) i.next();
         try
         {
            removeSchedule(lID.intValue());
         }
         catch (JMException jme)
         {
            log.error("Could not remove Schedule in stop providing", jme);
         }
      }
   }

   /**
    * Converts a string of method arguments (separated by colons) into
    * an array of string
    **/
   protected String[] getSignature(String pMethodSignature)
   {
      if (pMethodSignature == null || "".equals(pMethodSignature.trim()))
      {
         return new String[0];
      }

      StringTokenizer lTokenizer = new StringTokenizer(pMethodSignature, ",");
      String[] lReturn = new String[lTokenizer.countTokens()];

      int i = 0;

      while (lTokenizer.hasMoreTokens())
      {
         lReturn[i++] = lTokenizer.nextToken().trim();
      }

      return lReturn;
   }

   /**
    * Converts the given Data string to a date
    * where not value means 1/1/1970, "NOW" means
    * now (plus a second), an long value means time
    * in milliseconds since 1/1/1970 and a String is
    * a Date string which is intepreted by a Simple
    * Data Formatter.
    *
    * @param pStartDate the date
    * @param dateFormat the dateFormat, the locale is
    *        is used when null or blank
    */
   protected Date getStartDate(String pStartDate, String dateFormat)
   {
      pStartDate = pStartDate == null ? "" : pStartDate.trim();
      Date lReturn = null;

      if (pStartDate.equals(""))
      {
         lReturn = new Date(0);
      }

      else if (pStartDate.equals("NOW"))
      {
         lReturn = new Date(new Date().getTime() + 1000);
      }

      else
      {
         try
         {
            long lDate = new Long(pStartDate).longValue();
            lReturn = new Date(lDate);
         }
         catch (Exception e)
         {
            try
            {
               SimpleDateFormat dateFormatter = null;

               if (dateFormat == null || dateFormat.trim().length() == 0)
               {
                  dateFormatter = new SimpleDateFormat();
               }

               else
               {
                  dateFormatter = new SimpleDateFormat(dateFormat);
               }

               lReturn = dateFormatter.parse(pStartDate);
            }
            catch (Exception e2)
            {
               log.error("Could not parse given date string: " + pStartDate, e2);
               throw new InvalidParameterException("Schedulable Date is not of correct format");
            }
         }
      }
      log.debug("Initial Start Date is set to: " + lReturn);

      return lReturn;
   }

   // -------------------------------------------------------------------------
   // Methods
   // -------------------------------------------------------------------------

   public ObjectName getObjectName(MBeanServer pServer, ObjectName pName)
         throws MalformedObjectNameException
   {
      return pName == null ? DBScheduleProviderMBean.OBJECT_NAME : pName;
   }
}
