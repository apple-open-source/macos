/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.varia.scheduler;

import java.text.SimpleDateFormat;
import java.security.InvalidParameterException;
import java.util.Date;
import java.util.StringTokenizer;

import javax.management.JMException;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.ObjectName;

/**
 * This Provider adds a single Schedule to the Schedule Manager
 * but you can create more than one of this MBeans and each will
 * add a different Schedule even when the use the same Target.
 * ATTENTION: This is the provider you used in the older Scheduler
 * when you used a MBean as target.
 *
 * @jmx:mbean name="jboss:service=SingleScheduleProvider"
 *            extends="org.jboss.varia.scheduler.AbstractScheduleProviderMBean"
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.1 $
 */
public class SingleScheduleProvider
   extends AbstractScheduleProvider
   implements SingleScheduleProviderMBean
{

   // -------------------------------------------------------------------------
   // Constants
   // -------------------------------------------------------------------------
   
   // -------------------------------------------------------------------------
   // Members
   // -------------------------------------------------------------------------
   
   private ObjectName mSchedulableMBean;
   private String mSchedulableMBeanMethod;
   private String mSchedulableMBeanMethodName;
   private String[] mMethodSignature = new String[ 0 ];
   
   private SimpleDateFormat mDateFormatter;
   private Date mStartDate;
   private String mStartDateString;
   private long mSchedulePeriod;
   private long mInitialRepetitions;
   
   /** The ID of the Schedule used later to remove it later **/
   private int mScheduleID;
   
   // -------------------------------------------------------------------------
   // Constructors
   // -------------------------------------------------------------------------
   
   /**
    * Default (no-args) Constructor
    **/
   public SingleScheduleProvider()
   {
   }
   
   // -------------------------------------------------------------------------
   // SchedulerMBean Methods
   // -------------------------------------------------------------------------
   
   public void startProviding()
      throws JMException
   {
      mScheduleID = addSchedule(
         mSchedulableMBean,
         mSchedulableMBeanMethodName,
         mMethodSignature,
         mStartDate,
         mSchedulePeriod,
         (int) mInitialRepetitions
      );
   }
   
   public void stopProviding() {
      try {
         removeSchedule( mScheduleID );
      }
      catch( JMException jme ) {
         log.error( "Could not remove Schedule in stop providing", jme );
      }
   }
   
   /**
    * @jmx:managed-attribute
    *
    * @return Object Name of the Target MBean for the timer notifications
    */
   public String getTargetName() {
      return mSchedulableMBean == null ?
         null :
         mSchedulableMBean.toString();
   }
   
   /**
    * Sets the fully qualified JMX MBean Object Name of the Schedulable MBean to be called.
    *
    * @jmx:managed-attribute
    *
    * @param pTargetObjectName JMX MBean Object Name which should be called.
    *
    * @throws InvalidParameterException If the given value is an valid Object Name.
    */
   public void setTargetName( String pTargetObjectName )
      throws InvalidParameterException
   {
      if( pTargetObjectName == null ) {
         throw new InvalidParameterException( "Schedulable MBean must be specified" );
      }
      try {
         mSchedulableMBean = new ObjectName( pTargetObjectName );
      }
      catch( MalformedObjectNameException mone ) {
         log.error( "Schedulable MBean Object Name is malformed", mone );
         throw new InvalidParameterException( "Schedulable MBean is not correctly formatted" );
      }
   }
   
   /**
    * @return Method description of the target MBean to be called
    *
    * @jmx:managed-attribute
    **/
   public String getTargetMethod() {
      return mSchedulableMBeanMethod;
   }
   
   /**
    * Sets the method name to be called on the Schedulable MBean. It can optionally be
    * followed by an opening bracket, list of attributes (see below) and a closing bracket.
    * The list of attributes can contain:
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
    * <br>
    * An example could be: "doSomething( NOTIFICATION, REPETITIONS, java.lang.String )"
    * where the Scheduler will pass the timer's notification instance, the remaining
    * repetitions as int and a null to the MBean's doSomething() method which must
    * have the following signature: doSomething( javax.management.Notification, long,
    * java.lang.String ).
    *
    * @jmx:managed-attribute
    *
    * @param pTargetMethod Name of the method to be called optional followed
    *                                by method arguments (see above).
    *
    * @throws InvalidParameterException If the given value is not of the right
    *                                   format
    */
   public void setTargetMethod( String pTargetMethod  )
      throws InvalidParameterException
   {
      if( pTargetMethod == null ) {
         mSchedulableMBeanMethod = null;
         return;
      }
      int lIndex = pTargetMethod.indexOf( '(' );
      String lMethodName = "";
      if( lIndex < 0 ) {
         lMethodName = pTargetMethod.trim();
         mMethodSignature = new String[ 0 ];
      } else
      if( lIndex > 0 ) {
         lMethodName = pTargetMethod.substring( 0, lIndex ).trim();
      }
      if( lMethodName.equals( "" ) ) {
         lMethodName = "perform";
      }
      if( lIndex >= 0 ) {
         int lIndex2 = pTargetMethod.indexOf( ')' );
         if( lIndex2 < lIndex ) {
            throw new InvalidParameterException( "Schedulable MBean Method: closing bracket must be after opening bracket" );
         }
         if( lIndex2 < pTargetMethod.length() - 1 ) {
            String lRest = pTargetMethod.substring( lIndex2 + 1 ).trim();
            if( lRest.length() > 0 ) {
               throw new InvalidParameterException( "Schedulable MBean Method: nothing should be after closing bracket" );
            }
         }
         String lArguments = pTargetMethod.substring( lIndex + 1, lIndex2 ).trim();
         if( lArguments.equals( "" ) ) {
            mMethodSignature = new String[ 0 ];
         } else {
            StringTokenizer lTokenizer = new StringTokenizer( lArguments, "," );
            mMethodSignature = new String[ lTokenizer.countTokens() ];
            for( int i = 0; lTokenizer.hasMoreTokens(); i++ ) {
               mMethodSignature[ i ] = lTokenizer.nextToken().trim();
            }
         }
      }
      mSchedulableMBeanMethodName = lMethodName;
      mSchedulableMBeanMethod = pTargetMethod;
   }
   
   /**
    * @jmx:managed-attribute
    *
    * @return Schedule Period between two scheduled calls in Milliseconds. It will always
    *         be bigger than 0 except it returns -1 then the schedule is stopped.
    */
   public long getPeriod() {
      return mSchedulePeriod;
   }

   /**
    * Sets the Schedule Period between two scheduled call.
    *
    * @jmx:managed-attribute
    *
    * @param pPeriod Time between to scheduled calls (after the initial call) in Milliseconds.
    *                This value must be bigger than 0.
    *
    * @throws InvalidParameterException If the given value is less or equal than 0
    */
   public void setPeriod( long pPeriod ) {
      if( pPeriod <= 0 ) {
         throw new InvalidParameterException( "Schedulable Period may be not less or equals than 0" );
      }
      mSchedulePeriod = pPeriod;
   }

   /**
    * @jmx:managed-attribute
    *
    * @return the date format
    */
   public String getDateFormat()
   {
      if (mDateFormatter == null)
         mDateFormatter = new SimpleDateFormat();
      return mDateFormatter.toPattern();
   }
   
   /** 
    * Sets the date format used to parse date/times
    *
    * @jmx:managed-attribute
    *
    * @param dateFormat The date format when empty or null the locale is used to parse dates
    */
   public void setDateFormat(String dateFormat)
   {
      if (dateFormat == null || dateFormat.trim().length() == 0)
         mDateFormatter = new SimpleDateFormat();
      else
         mDateFormatter = new SimpleDateFormat(dateFormat);
   }
   
   /**
    * @jmx:managed-attribute
    *
    * @return Date (and time) of the first scheduled. For value see {@link #setInitialStartDate} 
    *         method.
    */
   public String getStartDate() {
      return mStartDateString;
   }
   
   /** 
    * Sets the first scheduled call. If the date is in the past the scheduler tries to find the
    * next available start date.
    *
    * @jmx:managed-attribute
    *
    * @param pStartDate Date when the initial call is scheduled. It can be either:
    *                   <ul>
    *                      <li>
    *                         NOW: date will be the current date (new Date()) plus 1 seconds
    *                      </li><li>
    *                         Date as String able to be parsed by SimpleDateFormat with default format
    *                      </li><li>
    *                         Date as String parsed using the date format attribute
    *                      </li><li>
    *                         Milliseconds since 1/1/1970
    *                      </li>
    *                   </ul>
    *                   If the date is in the past the Scheduler
    *                   will search a start date in the future with respect to the initial repe-
    *                   titions and the period between calls. This means that when you restart
    *                   the MBean (restarting JBoss etc.) it will start at the next scheduled
    *                   time. When no start date is available in the future the Scheduler will
    *                   not start.<br>
    *                   Example: if you start your Schedulable everyday at Noon and you restart
    *                   your JBoss server then it will start at the next Noon (the same if started
    *                   before Noon or the next day if start after Noon).
    */
   public void setStartDate( String pStartDate ) {
      mStartDateString = pStartDate == null ? "" : pStartDate.trim();
      if( mStartDateString.equals( "" ) ) {
         mStartDate = new Date( 0 );
      } else
      if( mStartDateString.equals( "NOW" ) ) {
         mStartDate = new Date( new Date().getTime() + 1000 );
      } else {
         try {
            long lDate = new Long( pStartDate ).longValue();
            mStartDate = new Date( lDate );
         }
         catch( Exception e ) {
            try {
               if( mDateFormatter == null ) {
                  mDateFormatter = new SimpleDateFormat();
               }
               mStartDate = mDateFormatter.parse( mStartDateString );
            }
            catch( Exception e2 ) {
               log.error( "Could not parse given date string: " + mStartDateString, e2 );
               throw new InvalidParameterException( "Schedulable Date is not of correct format" );
            }
         }
      }
      log.debug( "Initial Start Date is set to: " + mStartDate );
   }

   /**
    * @jmx:managed-attribute
    *
    * @return Number of scheduled calls initially. If -1 then there is not limit.
    */
   public long getRepetitions() {
      return mInitialRepetitions;
   }

   /**
    * Sets the initial number of scheduled calls.
    *
    * @jmx:managed-attribute
    *
    * @param pNumberOfCalls Initial Number of scheduled calls. If -1 then the number
    *                       is unlimted.
    *
    * @throws InvalidParameterException If the given value is less or equal than 0
    */
   public void setRepetitions( long pNumberOfCalls ) {
      if( pNumberOfCalls <= 0 ) {
         pNumberOfCalls = -1;
      }
      mInitialRepetitions = pNumberOfCalls;
   }
   
   // -------------------------------------------------------------------------
   // Methods
   // -------------------------------------------------------------------------
   
   public ObjectName getObjectName(
      MBeanServer pServer,
      ObjectName pName
   )
      throws MalformedObjectNameException
   {
      return pName == null ? SingleScheduleProviderMBean.OBJECT_NAME : pName;
   }
}
