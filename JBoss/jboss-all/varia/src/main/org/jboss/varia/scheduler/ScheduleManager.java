/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.varia.scheduler;

import java.util.ArrayList;
import java.util.Date;
import java.util.Hashtable;
import java.util.Iterator;

import javax.management.InstanceNotFoundException;
import javax.management.JMException;
import javax.management.MBeanException;
import javax.management.MBeanServer;
import javax.management.MalformedObjectNameException;
import javax.management.Notification;
import javax.management.NotificationListener;
import javax.management.ObjectName;
import javax.management.timer.TimerNotification;

import org.jboss.logging.Logger;
import org.jboss.system.ServiceMBeanSupport;

/**
 * Schedule Manager which manage the Schedule and their matching Timer notifications
 * and notification listeners.
 * Each provider has to register when it is started where in turn their startProviding()
 * method is called which allows him to add its Schedules because the Manager is now
 * ready.
 *
 * @jmx:mbean name="jboss:service=ScheduleMBean"
 *            extends="org.jboss.system.ServiceMBean"
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.2.2.4 $
 */
public class ScheduleManager
   extends ServiceMBeanSupport
   implements ScheduleManagerMBean
{

   // -------------------------------------------------------------------------
   // Constants
   // -------------------------------------------------------------------------
   
   /** Default Timer Object Name **/
   public static String DEFAULT_TIMER_NAME = "jboss:service=Timer";
   
   /** Counter for the Schedule Instance **/
   private static int sCounter = 0;
   
   private static final int NOTIFICATION = 0;
   private static final int DATE = 1;
   private static final int REPETITIONS = 2;
   private static final int SCHEDULER_NAME = 3;
   private static final int NULL = 4;
   private static final int ID = 5;
   
   // -------------------------------------------------------------------------
   // Members
   // -------------------------------------------------------------------------
   
   private String mTimerName = DEFAULT_TIMER_NAME;
   private ObjectName mTimer;
   
   private boolean mScheduleIsStarted = false;
   private boolean mWaitForNextCallToStop = false;
   private boolean mStartOnStart = false;
   private boolean mIsPaused = false;
   
   /**
    * List of registered providers to inform them when the
    * Schedule is stop / started or destroyed
    **/
   private ArrayList mProviders = new ArrayList();
   /** List of added Schedules **/
   private Hashtable mSchedules = new Hashtable();
   
   // -------------------------------------------------------------------------
   // Constructors
   // -------------------------------------------------------------------------
   
   /**
    * Default (no-args) Constructor
    **/
   public ScheduleManager()
   {
   }
   
   // -------------------------------------------------------------------------
   // SchedulerMBean Methods
   // -------------------------------------------------------------------------

   /**
    * Starts all the containing Schedules
    *
    * @jmx:managed-operation
    */
   public void startSchedules() {
      // Check if not already started
      if( !isStarted() ) {
         // Loop over all available Schedule Instance and start them now
         Iterator i = mSchedules.entrySet().iterator();
         while( i.hasNext() ) {
            ScheduleInstance lInstance = (ScheduleInstance) i.next();
            try {
               lInstance.start();
            }
            catch( JMException jme ) {
               log.error( "Could not start a Schedule", jme );
            }
         }
      }
      mScheduleIsStarted = true;
   }
   
   /**
    * Stops all the sontaining Schedules
    *
    * @param pDoItNow If true all the schedules are stopped immeditaly otherwise
    *                 it waits until the next notification is sent
    *
    * @jmx:managed-operation
    *
    * @param pDoItNow If true the schedule will be stopped without waiting for the next
    *                 scheduled call otherwise the next call will be performed before
    *                 the schedule is stopped.
    */
   public void stopSchedules( boolean pDoItNow )
   {
      // Check if it is already started
      if( isStarted() ) {
         // Loop over all available Schedule Instance and start them now
         Iterator i = mSchedules.entrySet().iterator();
         while( i.hasNext() ) {
            ScheduleInstance lInstance = (ScheduleInstance) i.next();
            try {
               lInstance.stop();
            }
            catch( JMException jme ) {
               log.error( "Could not stop a Schedule", jme );
            }
         }
      }
      mScheduleIsStarted = false;
   }
   
   /**
    * Stops the server right now and starts it right now.
    *
    * @jmx:managed-operation
    */
   public void restartSchedule() {
      stopSchedules( true );
      startSchedules();
   }
   
   /**
    * Register a Provider to make him available. In turn this
    * method calls "startProviding()" method on the Provider
    * to indicate that the Provider can start adding Schedules.
    *
    * @param pProviderObjectName Object Name of the Provider
    *
    * @jmx:managed-operation
    */
   public void registerProvider( String pProviderObjectName ) {
      if( pProviderObjectName == null ) {
         throw new RuntimeException( "Provider must not be null" );
      }
      int lIndex = mProviders.indexOf( pProviderObjectName );
      if( lIndex >= 0 ) {
         // Provider found then do nothing
         return;
      }
      // No provider found ==> a new provider
      mProviders.add( pProviderObjectName );
      // Initiate the Provider to provide schedules
      try {
         ObjectName lProviderName = new ObjectName( pProviderObjectName );
//AS         log.info( "Start Providing on: " + lProviderName + ", based on: " + pProviderObjectName );
         server.invoke(
            lProviderName,
            "startProviding",
            new Object[] {},
            new String[] {}
         );
      }
      catch( JMException jme ) {
         log.error( "Could not call startProviding() on the provider", jme );
         // Ignore Exceptions
      }
   }
   
   /**
    * Unregister a Provider which in turn calls "stopProviding()"
    * indicating to the Provider to remove all the Schedules.
    *
    * @param pProviderObjectName Object Name of the Provider
    *
    * @jmx:managed-operation
    */
   public void unregisterProvider( String pProviderObjectName ) {
      int lIndex = mProviders.indexOf( pProviderObjectName );
      if( lIndex < 0 ) {
         // Provider not found then do nothing
         return;
      }
      // Stop the Provider from providing schedules
      try {
         ObjectName lProviderName = new ObjectName( pProviderObjectName );
         server.invoke(
            lProviderName,
            "stopProviding",
            new Object[] {},
            new String[] {}
         );
      }
      catch( JMException jme ) {
         log.error( "Could not call stopProviding() on the provider", jme );
         // Ignore Exceptions
      }
      finally {
         // Finally remove the provider
         mProviders.remove( pProviderObjectName );
      }
   }
   
   /**
    * Adds a new Schedule to the Scheduler
    *
    * @param pTarget Object Name of the Target MBean
    * @param pMethodName Name of the method to be called
    * @param pMethodSignature List of Attributes of the method to be called
    *                         where ...
    * @param pStartDate Date when the schedule is started
    * @param pRepetitions Initial Number of repetitions
    *
    * @return Identification of the Schedule used later to remove it
    *         if necessary
    *
    * @jmx:managed-operation
    **/
   public int addSchedule(
      ObjectName pProvider,
      ObjectName pTarget,
      String pMethodName,
      String[] pMethodSignature,
      Date pStartDate,
      long pPeriod,
      int pRepetitions
   ) {
//AS      log.info( "addScheduler()" );
      ScheduleInstance lInstance = new ScheduleInstance(
         pProvider,
         pTarget,
         pMethodName,
         pMethodSignature,
         pStartDate,
         pRepetitions,
         pPeriod
      );
      // Only start it know when the Schedule Manager is started
      if( isStarted() ) {
         try {
            lInstance.start();
         }
         catch( JMException jme ) {
            log.error( "Could not start the Schedule", jme );
         }
      }
      int lID = lInstance.getID();
      mSchedules.put( new Integer( lID ), lInstance );
      
      return lID;
   }
   
   /**
    * Removes a Schedule so that no notification is sent anymore
    *
    * @param pIdentification Identification returned by {@link #addSchedule
    *                        addSchedule()} or {@link #getSchedules
    *                        getSchedules()}.
    *
    * @jmx:managed-operation
    **/
   public void removeSchedule( int pIdentification ) {
      ScheduleInstance lInstance = (ScheduleInstance) mSchedules.get( new Integer( pIdentification ) );
      try {
         lInstance.stop();
      }
      catch( JMException jme ) {
         log.error( "Could not stop a Schedule", jme );
      }
      mSchedules.remove( new Integer( pIdentification ) );
   }
   
   /**
    * Returns a list of the identifications of all registered
    * schedules
    *
    * @return List of Identifications separated by a ","
    **/
   public String getSchedules() {
      Iterator i = mSchedules.entrySet().iterator();
      StringBuffer lReturn = new StringBuffer();
      boolean lFirst = true;
      while( i.hasNext() ) {
         ScheduleInstance lInstance = (ScheduleInstance) i.next();
         if( lFirst ) {
            lReturn.append( lInstance.mIdentification + "" );
            lFirst = false;
         } else {
            lReturn.append( "," + lInstance.mIdentification );
         }
      }
      return lReturn.toString();
   }
   
   /**
    * @return True if all the Schedules are paused meaning that even when the notifications
    *                  are sent to the listener they are ignored. ATTENTION: this applies to all registered
    *                  Schedules and any notifications are lost during pausing
    **/
   public boolean isPaused() {
      return mIsPaused;
   }
   
   /**
    * Pauses or restarts the Schedules which either suspends the
    * notifications or start transfering them to the target
    *
    * @param pIsPaused True when the Schedules are paused or
    *                                       false when they resumes
    **/
   public void setPaused( boolean pIsPaused ) {
      mIsPaused = pIsPaused;
   }
   
   /**
    * @return True if the Schedule Manager is started
    **/
   public boolean isStarted() {
      return getState() == STARTED;
   }
   
   /**
    * @jmx:managed-attribute
    *
    * @return True if the Schedule when the Scheduler is started
    */
   public boolean isStartAtStartup() {
      return mStartOnStart;
   }
   
   /**
    * Set the scheduler to start when MBean started or not. Note that this method only
    * affects when the {@link #startService startService()} gets called (normally at
    * startup time.
    *
    * @jmx:managed-attribute
    *
    * @param pStartAtStartup True if Schedule has to be started at startup time
    */
   public void setStartAtStartup( boolean pStartAtStartup ) {
      mStartOnStart = pStartAtStartup;
   }
   
   /**
    * @jmx:managed-attribute
    *
    * @return Name of the Timer MBean used in here
    */
   public String getTimerName() {
      return mTimerName;
   }
   
   /**
    * @jmx:managed-attribute
    *
    * @param pTimerName Object Name of the Timer MBean to
    *                   be used. If null or not a valid ObjectName
    *                   the default will be used
    */
   public void setTimerName( String pTimerName ) {
      mTimerName = pTimerName;
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
      return pName == null ? OBJECT_NAME : pName;
   }
   
   // -------------------------------------------------------------------------
   // ServiceMBean - Methods
   // -------------------------------------------------------------------------
   
   /**
    * When Service is destroyed it will call the "unregisterProvider()"
    * on all register Providers to let them remove their Schedules and
    * being notified that they should stop providing.
    **/
   protected void destroyService() {
      // Unregister all providers
      Iterator i = mSchedules.entrySet().iterator();
      while( i.hasNext() ) {
         ScheduleInstance lInstance = (ScheduleInstance) i.next();
         unregisterProvider( lInstance.mProvider.toString() );
      }
   }
   
   /**
    * Creates the requested Timer if not already available
    * and start all added Schedules.
    * ATTENTION: the start of the schedules is not necessary when
    * the service is started but this method is also called when
    * the service is restarted and therefore schedules can be
    * available.
    **/
   protected void startService()
        throws Exception
   {
      // Create Timer MBean if need be
      
      try {
         mTimer = new ObjectName( mTimerName );
      }
      catch( MalformedObjectNameException mone ) {
         mTimer = new ObjectName( DEFAULT_TIMER_NAME );
      }
      
      if( !getServer().isRegistered( mTimer ) ) {
         getServer().createMBean( "javax.management.timer.Timer", mTimer );
      }
      if( !( (Boolean) getServer().getAttribute( mTimer, "Active" ) ).booleanValue() ) {
         // Now start the Timer
         getServer().invoke(
            mTimer,
            "start",
            new Object[] {},
            new String[] {}
         );
      }
      log.debug( "Start Schedules when Service is (re) started" );
      startSchedules();
   }
   
   /**
    * Stops all available Schedules.
    **/
   protected void stopService() {
      // Stop the schedule right now !!
      stopSchedules( true );
   }
   
   // -------------------------------------------------------------------------
   // Inner Classes
   // -------------------------------------------------------------------------
   
   /**
    * This listener is waiting for its Timer Notification and call the
    * appropriate method on the given Target (MBean) and count down the
    * number of remaining repetitions.
    **/
   public class MBeanListener
      implements NotificationListener
   {
      private final Logger log = Logger.getLogger( MBeanListener.class );
      
      private ScheduleInstance mSchedule;
      
      public MBeanListener( ScheduleInstance pSchedule ) {
         mSchedule = pSchedule;
      }
      
      public void handleNotification(
         Notification pNotification,
         Object pHandback
      ) {
         log.debug( "MBeanListener.handleNotification(), notification: " + pNotification );
         try {
            // If schedule is started invoke the schedule method on the Schedulable instance
            log.debug( "Scheduler is started: " + isStarted() );
            Date lTimeStamp = new Date( pNotification.getTimeStamp() );
            if( isStarted() ) {
               if( mSchedule.mRemainingRepetitions > 0 || mSchedule.mRemainingRepetitions < 0 ) {
                  if( mSchedule.mRemainingRepetitions > 0 ) {
                     mSchedule.mRemainingRepetitions--;
                  }
                  if( !mIsPaused ) {
                     Object[] lArguments = new Object[ mSchedule.mSchedulableMBeanArguments.length ];
                     for( int i = 0; i < lArguments.length; i++ ) {
                        switch( mSchedule.mSchedulableMBeanArguments[ i ] ) {
                           case ID:
                              lArguments[ i ] = ( (TimerNotification) pNotification ).getNotificationID();
                              break;
                           case NOTIFICATION:
                              lArguments[ i ] = pNotification;
                              break;
                           case DATE:
                              lArguments[ i ] = lTimeStamp;
                              break;
                           case REPETITIONS:
                              lArguments[ i ] = new Long( mSchedule.mRemainingRepetitions );
                              break;
                           case SCHEDULER_NAME:
                              lArguments[ i ] = getServiceName();
                              break;
                           default:
                              lArguments[ i ] = null;
                        }
                     }
                     log.debug( "MBean Arguments are: " + java.util.Arrays.asList( lArguments ) );
                     log.debug( "MBean Arguments Types are: " + java.util.Arrays.asList( mSchedule.mSchedulableMBeanArgumentTypes ) );
                     try {
                        getServer().invoke(
                           mSchedule.mTarget,
                           mSchedule.mMethodName,
                           lArguments,
                           mSchedule.mSchedulableMBeanArgumentTypes
                        );
                     }
                     catch( javax.management.JMRuntimeException jmre ) {
                        log.error( "Invoke of the Schedulable MBean failed", jmre );
                     }
                     catch( javax.management.JMException jme ) {
                        log.error( "Invoke of the Schedulable MBean failed", jme );
                     }
                     log.debug( "Remaining Repititions: " + mSchedule.mRemainingRepetitions +
                        ", wait for next call to stop: " + mWaitForNextCallToStop );
                     if( mSchedule.mRemainingRepetitions == 0 || mWaitForNextCallToStop ) {
                        mSchedule.stop();
                     }
                  }
               }
            }
            else {
               mSchedule.stop();
            }
         }
         catch( Exception e ) {
            log.error( "Handling a Scheduler call failed", e );
         }
      }
   }
   
   /**
    * Filter to ensure that each Scheduler only gets notified when it is supposed to.
    */
   private static class NotificationFilter implements javax.management.NotificationFilter {
      
      private Integer mId;
      
      /**
       * Create a Filter.
       * @param id the Scheduler id
       */
      public NotificationFilter( Integer pId ){
         mId = pId;
      }
      
      /**
       * Determine if the notification should be sent to this Scheduler
       */
      public boolean isNotificationEnabled( Notification pNotification ) {
         if( pNotification instanceof TimerNotification ) {
            TimerNotification lTimerNotification = (TimerNotification) pNotification;
            return lTimerNotification.getNotificationID().equals( mId );
         }
         return false;
      }
   }
   
   /**
    * Represents a single Schedule which can be started and stop
    * if necessary.
    **/
   private class ScheduleInstance {
      
      private final Logger log = Logger.getLogger( ScheduleInstance.class );
      private int mIdentification;
      private MBeanListener mListener;
      
      public int mNotificationID;
      public ObjectName mProvider;
      public ObjectName mTarget;
      public int mInitialRepetitions;
      public int mRemainingRepetitions = 0;
      public Date mStartDate;
      public long mPeriod;
      public String mMethodName;
      public int[] mSchedulableMBeanArguments;
      public String[] mSchedulableMBeanArgumentTypes;
      
      public ScheduleInstance(
         ObjectName pProvider,
         ObjectName pTarget,
         String pMethodName,
         String[] pMethodArguments,
         Date pStartDate,
         int pRepetitions,
         long pPeriod
      ) {
         mProvider = pProvider;
         mTarget = pTarget;
         mInitialRepetitions = pRepetitions;
         mStartDate = pStartDate;
         mPeriod = pPeriod;
         mMethodName = pMethodName;
         mSchedulableMBeanArguments = new int[ pMethodArguments.length ];
         mSchedulableMBeanArgumentTypes = new String[ pMethodArguments.length ];
         for( int i = 0; i < pMethodArguments.length; i++ ) {
            String lToken = pMethodArguments[ i ];
            if( lToken.equals( "ID" ) ) {
               mSchedulableMBeanArguments[ i ] = ID;
               mSchedulableMBeanArgumentTypes[ i ] = Integer.class.getName();
            } else
            if( lToken.equals( "NOTIFICATION" ) ) {
               mSchedulableMBeanArguments[ i ] = NOTIFICATION;
               mSchedulableMBeanArgumentTypes[ i ] = Notification.class.getName();
            } else
            if( lToken.equals( "DATE" ) ) {
               mSchedulableMBeanArguments[ i ] = DATE;
               mSchedulableMBeanArgumentTypes[ i ] = Date.class.getName();
            } else
            if( lToken.equals( "REPETITIONS" ) ) {
               mSchedulableMBeanArguments[ i ] = REPETITIONS;
               mSchedulableMBeanArgumentTypes[ i ] = Long.TYPE.getName();
            } else
            if( lToken.equals( "SCHEDULER_NAME" ) ) {
               mSchedulableMBeanArguments[ i ] = SCHEDULER_NAME;
               mSchedulableMBeanArgumentTypes[ i ] = ObjectName.class.getName();
            } else {
               mSchedulableMBeanArguments[ i ] = NULL;
               //AS ToDo: maybe later to check if this class exists !
               mSchedulableMBeanArgumentTypes[ i ] = lToken;
            }
         }
         mIdentification = ( sCounter++ );
      }
      
      /**
       * Starts the Schedule by adding itself to the timer
       * and registering its listener to get the notifications
       * and hand over to the target
       *
       * @return The notification identification
       **/
      public void start()
         throws JMException
      {
         Date lStartDate = null;
         // Check if initial start date is in the past
         if( mStartDate.getTime() < new Date().getTime() ) {
            // If then first check if a repetition is in the future
            long lNow = new Date().getTime() + 100;
            int lSkipRepeats = (int) ( ( lNow - mStartDate.getTime() ) / mPeriod ) + 1;
            log.debug( "Old start date: " + mStartDate + ", now: " + new Date( lNow ) + ", Skip repeats: " + lSkipRepeats );
            if( mInitialRepetitions > 0 ) {
               // If not infinit loop
               if( lSkipRepeats >= mInitialRepetitions ) {
                  // No repetition left -> exit
                  log.warn( "No repetitions left because start date is in the past and could " +
                     "not be reached by Initial Repetitions * Schedule Period" );
                  return;
               } else {
                  // Reduce the missed hits
                  mRemainingRepetitions = mInitialRepetitions - lSkipRepeats;
               }
            } else {
               if( mInitialRepetitions == 0 ) {
                  mRemainingRepetitions = 0;
               } else {
                  mRemainingRepetitions = -1;
               }
            }
            lStartDate = new Date( mStartDate.getTime() + ( lSkipRepeats * mPeriod ) );
         } else {
            lStartDate = mStartDate;
            mRemainingRepetitions = mInitialRepetitions;
         }
         mNotificationID = ( (Integer) getServer().invoke(
            mTimer,
            "addNotification",
            new Object[] {
               "Schedule",
               "Scheduler Notification",
               null,       // User Object
               lStartDate,
               new Long( mPeriod ),
               mRemainingRepetitions < 0 ?
                  new Long( 0 ) :
                  new Long( mRemainingRepetitions )
            },
            new String[] {
               String.class.getName(),
               String.class.getName(),
               Object.class.getName(),
               Date.class.getName(),
               Long.TYPE.getName(),
               Long.TYPE.getName()
            }
         ) ).intValue();
         // Register the notification listener at the MBeanServer
         mListener = new MBeanListener( this );
         getServer().addNotificationListener(
            mTimer,
            mListener,
            new NotificationFilter( new Integer( mNotificationID ) ),
            // No object handback necessary
            null
         );
         log.info( "start(), add Notification to Timer with ID: " + mNotificationID );
      }
      
      /**
       * Stops the Schedule by remove itself from the timer
       * and removing the listener
       **/
      public void stop()
         throws JMException
      {
         log.debug( "stopSchedule(), notification id: " + mNotificationID );
         getServer().removeNotificationListener(
            mTimer,
            mListener
         );
         try {
            getServer().invoke(
               mTimer,
               "removeNotification",
               new Object[] {
                  new Integer( mNotificationID )
               },
               new String[] {
                  Integer.class.getName()
               }
            );
         }
         catch( MBeanException mbe ) {
            Exception e = mbe.getTargetException();
            // If target exception is InstanceNotFoundException then
            // the notification is already removed so ignore it
            if( !( e instanceof InstanceNotFoundException ) ) {
               throw mbe;
            }
         }
      }
      
      public int getID() {
         return mIdentification;
      }
   }
}
