/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.varia.scheduler;

import java.lang.reflect.Constructor;
import java.text.SimpleDateFormat;
import java.security.InvalidParameterException;
import java.util.Date;
import java.util.StringTokenizer;
import java.util.Vector;

import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.Notification;
import javax.management.timer.TimerNotification;
import javax.management.NotificationListener;
import javax.management.ObjectName;

import org.jboss.logging.Logger;
import org.jboss.system.ServiceMBeanSupport;
import org.jboss.mx.loading.ClassLoaderUtils;

/**
 * Scheduler Instance to allow clients to run this as a scheduling service for
 * any Schedulable instances.
 * <br>
 * ATTENTION: The scheduler instance only allows to run one schedule at a time.
 * Therefore when you want to run two schedules create to instances with this
 * MBean. Suggested Object Name for the MBean are:<br>
 * :service=Scheduler,schedule=<you schedule name><br>
 * This way you should not run into a name conflict.
 *
 * @jmx:mbean name="jboss:service=Scheduler"
 *            extends="org.jboss.system.ServiceMBean"
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @author Cameron (camtabor)
 * @version $Revision: 1.3.2.5 $
 *
 * <p><b>Revisions:</b></p>
 * <p><b>20010814 Cameron:</b>
 * <ul>
 * <li>Checks if the TimerMBean is already loaded</li>
 * <li>Created a SchedulerNotificationFilter so that each Scheduler only
 *     get its notifications</li>
 * <li>Stop was broken because removeNotification( Integer ) was broken</li>
 * </ul>
 * </p>
 * <p><b>20011026 Andy:</b>
 * <ul>
 * <li>Move the SchedulerNotificationFilter to become an internal class
 *     and renamed to NotificationFilter</li>
 * <li>MBean is not bind/unbind to JNDI server anymore</li>
 * </ul>
 * </p>
 * <p><b>20020117 Andy:</b>
 * <ul>
 * <li>Change the behaviour when the Start Date is in the past. Now the
 *     Scheduler will behave as the Schedule is never stopped and find
 *     the next available time to start with respect to the settings.
 *     Therefore you can restart JBoss without adjust your Schedule
 *     every time. BUT you will still loose the calls during the Schedule
 *     was down.</li>
 * <li>Added parsing capabilities to setInitialStartDate. Now NOW: current time,
 *     and a string in a format the SimpleDataFormat understand in your environment
 *     (US: m/d/yy h:m a) but of course the time in ms since 1/1/1970.</li>
 * <li>Some fixes like the stopping a Schedule even if it already stopped etc.</li>
 * </ul>
 * </p>
 * <p><b>20020118 Andy:</b>
 * <ul>
 * <li>Added the ability to call another MBean instead of an instance of the
 *     given Schedulable class. Use setSchedulableMBean() to specify the JMX
 *     Object Name pointing to the given MBean. Then if the MBean does not
 *     contain the same method as the Schedulable instance you have to specify
 *     the method with setSchedulableMBeanMethod(). There you can use some
 *     constants.</li>
 * </p>
 * <p><b>20020119 Andy:</b>
 * <ul>
 * <li>Added a helper method isActive()</li>
 * <li>Fixed a bug not indicating that no MBean is used when setSchedulableClass()
 *     is called.</li>
 * <li>Fixed a bug therefore that when NOW is set as initial start date a restart
 *     of the Schedule will reset the start date to the current date</li>
 * <li>Fixed a bug because the start date was update during recalculation of the
 *     start date when the original start date is in the past (see above). With this
 *     you could restart the schedule even after all hits were fired when the
 *     schedule was not started immediately after the initial start date was set.</li>
 * </p>
 */
public class Scheduler
   extends ServiceMBeanSupport
   implements SchedulerMBean
{

   // -------------------------------------------------------------------------
   // Constants
   // -------------------------------------------------------------------------

   public static String JNDI_NAME = "scheduler:domain";
   public static String JMX_NAME = "scheduler";
   /** Default Timer Object Name **/
   public static String DEFAULT_TIMER_NAME = "jboss:service=Timer";

   private static final int NOTIFICATION = 0;
   private static final int DATE = 1;
   private static final int REPETITIONS = 2;
   private static final int SCHEDULER_NAME = 3;
   private static final int NULL = 4;

   // -------------------------------------------------------------------------
   // Members
   // -------------------------------------------------------------------------

   private long mActualSchedulePeriod;
   private long mRemainingRepetitions = 0;
   private int mActualSchedule = -1;
   private String mTimerName = DEFAULT_TIMER_NAME;
   private ObjectName mTimer;
   private Schedulable mSchedulable;
//as   private Object mSchedulableMBeanObjectName;

   private boolean mScheduleIsStarted = false;
   private boolean mWaitForNextCallToStop = false;
   private boolean mStartOnStart = false;
   private boolean mIsRestartPending = true;

   // Pending values which can be different to the actual ones
   private boolean mUseMBean = false;

   private Class mSchedulableClass;
   private String mSchedulableArguments;
   private String[] mSchedulableArgumentList = new String[0];
   private String mSchedulableArgumentTypes;
   private Class[] mSchedulableArgumentTypeList = new Class[0];

   private ObjectName mSchedulableMBean;
   private String mSchedulableMBeanMethod;
   private String mSchedulableMBeanMethodName;
   private int[] mSchedulableMBeanArguments = new int[0];
   private String[] mSchedulableMBeanArgumentTypes = new String[0];

   private SimpleDateFormat mDateFormatter;
   private Date mStartDate;
   private String mStartDateString;
   private boolean mStartDateIsNow;
   private long mSchedulePeriod;
   private long mInitialRepetitions;

   private NotificationListener listener;

   // -------------------------------------------------------------------------
   // Constructors
   // -------------------------------------------------------------------------

   /**
    * Default (no-args) Constructor
    **/
   public Scheduler()
   {
   }

   /**
    * Constructor with the necessary attributes to be set
    *
    * @param pName Name of the MBean
    **/
   public Scheduler(
      String pSchedulableClass,
      String pInitArguments,
      String pInitTypes,
      String pInitialStartDate,
      long pSchedulePeriod,
      long pNumberOfRepetitions
      )
   {
      setStartAtStartup(true);
      setSchedulableClass(pSchedulableClass);
      setSchedulableArguments(pInitArguments);
      setSchedulableArgumentTypes(pInitTypes);
      setInitialStartDate(pInitialStartDate);
      setSchedulePeriod(pSchedulePeriod);
      setInitialRepetitions(pNumberOfRepetitions);
   }

   /**
    * Constructor with the necessary attributes to be set
    *
    * @param pName Name of the MBean
    **/
   public Scheduler(
      String pSchedulableClass,
      String pInitArguments,
      String pInitTypes,
      String pDateFormat,
      String pInitialStartDate,
      long pSchedulePeriod,
      long pNumberOfRepetitions
      )
   {
      setStartAtStartup(true);
      setSchedulableClass(pSchedulableClass);
      setSchedulableArguments(pInitArguments);
      setSchedulableArgumentTypes(pInitTypes);
      setDateFormat(pDateFormat);
      setInitialStartDate(pInitialStartDate);
      setSchedulePeriod(pSchedulePeriod);
      setInitialRepetitions(pNumberOfRepetitions);
   }

   // -------------------------------------------------------------------------
   // SchedulerMBean Methods
   // -------------------------------------------------------------------------

   /**
    * Starts the schedule if the schedule is stopped otherwise nothing will happen.
    * The Schedule is immediately set to started even the first call is in the
    * future.
    *
    * @jmx:managed-operation
    *
    * @throws InvalidParameterException If any of the necessary values are not set
    *                                   or invalid (especially for the Schedulable
    *                                   class attributes).
    */
   public void startSchedule()
   {
      // Check if not already started
      if (!isStarted())
      {
         try
         {
            // Check the given attributes if correct
            if (mUseMBean)
            {
               if (mSchedulableMBean == null)
               {
                  log.debug("Schedulable MBean Object Name is not set");
                  throw new InvalidParameterException(
                     "Schedulable MBean must be set"
                  );
               }
               if (mSchedulableMBeanMethodName == null)
               {
                  mSchedulableMBeanMethodName = "perform";
                  mSchedulableMBeanArguments = new int[]{DATE, REPETITIONS};
                  mSchedulableMBeanArgumentTypes = new String[]{
                     Date.class.getName(),
                     Integer.TYPE.getName()
                  };
               }
            }
            else
            {
               if (mSchedulableClass == null)
               {
                  log.debug("Schedulable Class is not set");
                  throw new InvalidParameterException(
                     "Schedulable Class must be set"
                  );
               }
               if (mSchedulableArgumentList.length != mSchedulableArgumentTypeList.length)
               {
                  log.debug("Schedulable Class Arguments and Types do not match in length");
                  throw new InvalidParameterException(
                     "Schedulable Class Arguments and Types do not match in length"
                  );
               }
            }
            if (mSchedulePeriod <= 0)
            {
               log.debug("Schedule Period is less than 0 (ms)");
               throw new InvalidParameterException(
                  "Schedule Period must be set and greater than 0 (ms)"
               );
            }
            if (!mUseMBean)
            {
               // Create all the Objects for the Constructor to be called
               Object[] lArgumentList = new Object[mSchedulableArgumentTypeList.length];
               try
               {
                  for (int i = 0; i < mSchedulableArgumentTypeList.length; i++)
                  {
                     Class lClass = mSchedulableArgumentTypeList[i];
                     if (lClass == Boolean.TYPE)
                     {
                        lArgumentList[i] = new Boolean(mSchedulableArgumentList[i]);
                     }
                     else if (lClass == Integer.TYPE)
                     {
                        lArgumentList[i] = new Integer(mSchedulableArgumentList[i]);
                     }
                     else if (lClass == Long.TYPE)
                     {
                        lArgumentList[i] = new Long(mSchedulableArgumentList[i]);
                     }
                     else if (lClass == Short.TYPE)
                     {
                        lArgumentList[i] = new Short(mSchedulableArgumentList[i]);
                     }
                     else if (lClass == Float.TYPE)
                     {
                        lArgumentList[i] = new Float(mSchedulableArgumentList[i]);
                     }
                     else if (lClass == Double.TYPE)
                     {
                        lArgumentList[i] = new Double(mSchedulableArgumentList[i]);
                     }
                     else if (lClass == Byte.TYPE)
                     {
                        lArgumentList[i] = new Byte(mSchedulableArgumentList[i]);
                     }
                     else if (lClass == Character.TYPE)
                     {
                        lArgumentList[i] = new Character(mSchedulableArgumentList[i].charAt(0));
                     }
                     else
                     {
                        Constructor lConstructor = lClass.getConstructor(new Class[]{String.class});
                        lArgumentList[i] = lConstructor.newInstance(new Object[]{mSchedulableArgumentList[i]});
                     }
                  }
               }
               catch (Exception e)
               {
                  log.error("Could not load or create constructor argument", e);
                  throw new InvalidParameterException("Could not load or create a constructor argument");
               }
               try
               {
                  // Check if constructor is found
                  Constructor lSchedulableConstructor = mSchedulableClass.getConstructor(mSchedulableArgumentTypeList);
                  // Create an instance of it
                  mSchedulable = (Schedulable) lSchedulableConstructor.newInstance(lArgumentList);
               }
               catch (Exception e)
               {
                  log.error("Could not find the constructor or create Schedulable instance", e);
                  throw new InvalidParameterException("Could not find the constructor or create the Schedulable Instance");
               }
            }

            mRemainingRepetitions = mInitialRepetitions;
            mActualSchedulePeriod = mSchedulePeriod;
            Date lStartDate = null;
            // Register the Schedule at the Timer
            // If start date is NOW then take the current date
            if (mStartDateIsNow)
            {
               mStartDate = new Date(new Date().getTime() + 1000);
               lStartDate = mStartDate;
            }
            else
            {
               // Check if initial start date is in the past
               if (mStartDate.getTime() < new Date().getTime())
               {
                  // If then first check if a repetition is in the future
                  long lNow = new Date().getTime() + 100;
                  long lSkipRepeats = ((lNow - mStartDate.getTime()) / mActualSchedulePeriod) + 1;
                  log.debug("Old start date: " + mStartDate + ", now: " + new Date(lNow) + ", Skip repeats: " + lSkipRepeats);
                  if (mRemainingRepetitions > 0)
                  {
                     // If not infinit loop
                     if (lSkipRepeats >= mRemainingRepetitions)
                     {
                        // No repetition left -> exit
                        log.info("No repetitions left because start date is in the past and could " +
                           "not be reached by Initial Repetitions * Schedule Period");
                        return;
                     }
                     else
                     {
                        // Reduce the missed hits
                        mRemainingRepetitions -= lSkipRepeats;
                     }
                  }
                  lStartDate = new Date(mStartDate.getTime() + (lSkipRepeats * mActualSchedulePeriod));
               }
               else
               {
                  lStartDate = mStartDate;
               }
            }
            log.debug("Schedule initial call to: " + lStartDate + ", remaining repetitions: " + mRemainingRepetitions);
            // Add an initial call
            mActualSchedule = ((Integer) getServer().invoke(
               mTimer,
               "addNotification",
               new Object[]{
                  "Schedule",
                  "Scheduler Notification",
                  null, // User Object
                  lStartDate,
                  new Long(mActualSchedulePeriod),
                  mRemainingRepetitions < 0 ?
               new Long(0) :
               new Long(mRemainingRepetitions)
               },
               new String[]{
                  String.class.getName(),
                  String.class.getName(),
                  Object.class.getName(),
                  Date.class.getName(),
                  Long.TYPE.getName(),
                  Long.TYPE.getName()
               }
            )).intValue();
            if (mUseMBean)
            {
               listener = new MBeanListener(mSchedulableMBean);
            }
            else
            {
               listener = new Listener(mSchedulable);
            }
            // Register the notification listener at the MBeanServer
            getServer().addNotificationListener(
               mTimer,
               listener,
               new Scheduler.NotificationFilter(new Integer(mActualSchedule)),
               // No object handback necessary
               null
            );
            mScheduleIsStarted = true;
            mIsRestartPending = false;
         }
         catch (Exception e)
         {
            log.error("operation failed", e);
         }
      }
   }

   /**
    * Stops the schedule because it is either not used anymore or to restart it with
    * new values.
    *
    * @jmx:managed-operation
    *
    * @param pDoItNow If true the schedule will be stopped without waiting for the next
    *                 scheduled call otherwise the next call will be performed before
    *                 the schedule is stopped.
    */
   public void stopSchedule(boolean pDoItNow)
   {
      try
      {
         if (mActualSchedule < 0)
         {
            mScheduleIsStarted = false;
            mWaitForNextCallToStop = false;
            return;
         }
         if (pDoItNow)
         {
            mWaitForNextCallToStop = false;
            // Remove notification listener now
            if (listener != null)
            {
               getServer().removeNotificationListener(
                  mTimer,
                  listener
               );
               listener = null;
            }
            log.debug("stopSchedule(), schedule id: " + mActualSchedule);
            getServer().invoke(
               mTimer,
               "removeNotification",
               new Object[]{
                  new Integer(mActualSchedule)
               },
               new String[]{
                  Integer.class.getName()
               }
            );
            log.debug("stopSchedule(), removed schedule id: " + mActualSchedule);
            mActualSchedule = -1;
            mScheduleIsStarted = false;
         }
         else
         {
            mWaitForNextCallToStop = true;
         }
      }
      catch (Exception e)
      {
         log.error("operation failed", e);
      }
   }

   /**
    * Stops the server right now and starts it right now.
    *
    * @jmx:managed-operation
    */
   public void restartSchedule()
   {
      stopSchedule(true);
      startSchedule();
   }

   /**
    * @jmx:managed-attribute
    *
    * @return Full qualified Class name of the schedulable class called by the schedule or
    *         null if not set.
    */
   public String getSchedulableClass()
   {
      if (mSchedulableClass == null)
      {
         return null;
      }
      return mSchedulableClass.getName();
   }

   /**
    * Sets the fully qualified Class name of the Schedulable Class being called by the
    * Scheduler. Must be set before the Schedule is started. Please also set the
    * {@link #setSchedulableArguments} and {@link #setSchedulableArgumentTypes}.
    *
    * @jmx:managed-attribute
    *
    * @param pSchedulableClass Fully Qualified Schedulable Class.
    *
    * @throws InvalidParameterException If the given value is not a valid class or cannot
    *                                   be loaded by the Scheduler or is not of instance
    *                                   Schedulable.
    */
   public void setSchedulableClass(String pSchedulableClass)
      throws InvalidParameterException
   {
      if (pSchedulableClass == null || pSchedulableClass.equals(""))
      {
         throw new InvalidParameterException("Schedulable Class cannot be empty or undefined");
      }
      try
      {
         // Try to load the Schedulable Class
         mSchedulableClass = Thread.currentThread().getContextClassLoader().loadClass(pSchedulableClass);
         // Check if instance of Schedulable
         Class[] lInterfaces = mSchedulableClass.getInterfaces();
         boolean lFound = false;
         for (int i = 0; i < lInterfaces.length; i++)
         {
            if (lInterfaces[i] == Schedulable.class)
            {
               lFound = true;
               break;
            }
         }
         if (!lFound)
         {
            String msg = "Given class " + pSchedulableClass + " is not instance of Schedulable";
            StringBuffer info = new StringBuffer(msg);
            info.append("\nThe SchedulableClass info:");
            ClassLoaderUtils.displayClassInfo(mSchedulableClass, info);
            info.append("\nSchedulable.class info:");
            ClassLoaderUtils.displayClassInfo(Schedulable.class, info);
            log.debug(info.toString());
            throw new InvalidParameterException(msg);
         }
      }
      catch (ClassNotFoundException e)
      {
         log.info("Failed to find: "+pSchedulableClass, e);
         throw new InvalidParameterException(
            "Given class " + pSchedulableClass + " is not  not found"
         );
      }
      mIsRestartPending = true;
      mUseMBean = false;
   }

   /**
    * @jmx:managed-attribute
    *
    * @return Comma seperated list of Constructor Arguments used to instantiate the
    *         Schedulable class instance. Right now only basic data types, String and
    *         Classes with a Constructor with a String as only argument are supported.
    */
   public String getSchedulableArguments()
   {
      return mSchedulableArguments;
   }

   /**
    * @jmx:managed-attribute
    *
    * Sets the comma seperated list of arguments for the Schedulable class. Note that
    * this list must have as many elements as the Schedulable Argument Type list otherwise
    * the start of the Scheduler will fail. Right now only basic data types, String and
    * Classes with a Constructor with a String as only argument are supported.
    *
    * @param pArgumentList List of arguments used to create the Schedulable intance. If
    *                      the list is null or empty then the no-args constructor is used.
    */
   public void setSchedulableArguments(String pArgumentList)
   {
      if (pArgumentList == null || pArgumentList.equals(""))
      {
         mSchedulableArgumentList = new String[0];
      }
      else
      {
         StringTokenizer lTokenizer = new StringTokenizer(pArgumentList, ",");
         Vector lList = new Vector();
         while (lTokenizer.hasMoreTokens())
         {
            String lToken = lTokenizer.nextToken().trim();
            if (lToken.equals(""))
            {
               lList.add("null");
            }
            else
            {
               lList.add(lToken);
            }
         }
         mSchedulableArgumentList = (String[]) lList.toArray(new String[0]);
      }
      mSchedulableArguments = pArgumentList;
      mIsRestartPending = true;
   }

   /**
    * @jmx:managed-attribute
    *
    * @return A comma seperated list of Argument Types which should match the list of
    *         arguments.
    */
   public String getSchedulableArgumentTypes()
   {
      return mSchedulableArgumentTypes;
   }

   /**
    * Sets the comma seperated list of argument types for the Schedulable class. This will
    * be used to find the right constructor and to created the right instances to call the
    * constructor with. This list must have as many elements as the Schedulable Arguments
    * list otherwise the start of the Scheduler will fail. Right now only basic data types,
    * String and Classes with a Constructor with a String as only argument are supported.
    *
    * @jmx:managed-attribute
    *
    * @param pTypeList List of arguments used to create the Schedulable intance. If
    *                  the list is null or empty then the no-args constructor is used.
    *
    * @throws InvalidParameterException If the given list contains a unknow datat type.
    */
   public void setSchedulableArgumentTypes(String pTypeList)
      throws InvalidParameterException
   {
      if (pTypeList == null || pTypeList.equals(""))
      {
         mSchedulableArgumentTypeList = new Class[0];
      }
      else
      {
         StringTokenizer lTokenizer = new StringTokenizer(pTypeList, ",");
         Vector lList = new Vector();
         while (lTokenizer.hasMoreTokens())
         {
            String lToken = lTokenizer.nextToken().trim();
            // Get the class
            Class lClass = null;
            if (lToken.equals("short"))
            {
               lClass = Short.TYPE;
            }
            else if (lToken.equals("int"))
            {
               lClass = Integer.TYPE;
            }
            else if (lToken.equals("long"))
            {
               lClass = Long.TYPE;
            }
            else if (lToken.equals("byte"))
            {
               lClass = Byte.TYPE;
            }
            else if (lToken.equals("char"))
            {
               lClass = Character.TYPE;
            }
            else if (lToken.equals("float"))
            {
               lClass = Float.TYPE;
            }
            else if (lToken.equals("double"))
            {
               lClass = Double.TYPE;
            }
            else if (lToken.equals("boolean"))
            {
               lClass = Boolean.TYPE;
            }
            if (lClass == null)
            {
               try
               {
                  // Load class to check if available
                  lClass = Thread.currentThread().getContextClassLoader().loadClass(lToken);
               }
               catch (ClassNotFoundException cnfe)
               {
                  throw new InvalidParameterException(
                     "The argument type: " + lToken + " is not a valid class or could not be found"
                  );
               }
            }
            lList.add(lClass);
         }
         mSchedulableArgumentTypeList = (Class[]) lList.toArray(new Class[0]);
      }
      mSchedulableArgumentTypes = pTypeList;
      mIsRestartPending = true;
   }

   /**
    * @jmx:managed-attribute
    *
    * @return Object Name if a Schedulalbe MBean is set
    */
   public String getSchedulableMBean()
   {
      return mSchedulableMBean == null ?
         null :
         mSchedulableMBean.toString();
   }

   /**
    * Sets the fully qualified JMX MBean name of the Schedulable MBean to be called.
    * <b>Attention: </b>if set the all values set by {@link #setSchedulableClass},
    * {@link #setSchedulableArguments} and {@link #setSchedulableArgumentTypes} are
    * cleared and not used anymore. Therefore only use either Schedulable Class or
    * Schedulable MBean. If {@link #setSchedulableMBeanMethod} is not set then the
    * schedule method as in the {@link Schedulable#perform} will be called with the
    * same arguments. Also note that the Object Name will not be checked if the
    * MBean is available. If the MBean is not available it will not be called but
    * the remaining repetitions will be decreased.
    *
    * @jmx:managed-attribute
    *
    * @param pSchedulableMBean JMX MBean Object Name which should be called.
    *
    * @throws InvalidParameterException If the given value is an valid Object Name.
    */
   public void setSchedulableMBean(String pSchedulableMBean)
      throws InvalidParameterException
   {
      if (pSchedulableMBean == null)
      {
         throw new InvalidParameterException("Schedulable MBean must be specified");
      }
      try
      {
         mSchedulableMBean = new ObjectName(pSchedulableMBean);
         mUseMBean = true;
      }
      catch (MalformedObjectNameException mone)
      {
         log.error("Schedulable MBean Object Name is malformed", mone);
         throw new InvalidParameterException("Schedulable MBean is not correctly formatted");
      }
   }

   /**
    * @return Schedulable MBean Method description if set
    **/
   public String getSchedulableMBeanMethod()
   {
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
    * @param pSchedulableMBeanMethod Name of the method to be called optional followed
    *                                by method arguments (see above).
    *
    * @throws InvalidParameterException If the given value is not of the right
    *                                   format
    */
   public void setSchedulableMBeanMethod(String pSchedulableMBeanMethod)
      throws InvalidParameterException
   {
      if (pSchedulableMBeanMethod == null)
      {
         mSchedulableMBeanMethod = null;
         return;
      }
      int lIndex = pSchedulableMBeanMethod.indexOf('(');
      String lMethodName = "";
      if (lIndex < 0)
      {
         lMethodName = pSchedulableMBeanMethod.trim();
         mSchedulableMBeanArguments = new int[0];
         mSchedulableMBeanArgumentTypes = new String[0];
      }
      else if (lIndex > 0)
      {
         lMethodName = pSchedulableMBeanMethod.substring(0, lIndex).trim();
      }
      if (lMethodName.equals(""))
      {
         lMethodName = "perform";
      }
      if (lIndex >= 0)
      {
         int lIndex2 = pSchedulableMBeanMethod.indexOf(')');
         if (lIndex2 < lIndex)
         {
            throw new InvalidParameterException("Schedulable MBean Method: closing bracket must be after opening bracket");
         }
         if (lIndex2 < pSchedulableMBeanMethod.length() - 1)
         {
            String lRest = pSchedulableMBeanMethod.substring(lIndex2 + 1).trim();
            if (lRest.length() > 0)
            {
               throw new InvalidParameterException("Schedulable MBean Method: nothing should be after closing bracket");
            }
         }
         String lArguments = pSchedulableMBeanMethod.substring(lIndex + 1, lIndex2).trim();
         if (lArguments.equals(""))
         {
            mSchedulableMBeanArguments = new int[0];
            mSchedulableMBeanArgumentTypes = new String[0];
         }
         else
         {
            StringTokenizer lTokenizer = new StringTokenizer(lArguments, ",");
            mSchedulableMBeanArguments = new int[lTokenizer.countTokens()];
            mSchedulableMBeanArgumentTypes = new String[lTokenizer.countTokens()];
            for (int i = 0; lTokenizer.hasMoreTokens(); i++)
            {
               String lToken = lTokenizer.nextToken().trim();
               if (lToken.equals("NOTIFICATION"))
               {
                  mSchedulableMBeanArguments[i] = NOTIFICATION;
                  mSchedulableMBeanArgumentTypes[i] = Notification.class.getName();
               }
               else if (lToken.equals("DATE"))
               {
                  mSchedulableMBeanArguments[i] = DATE;
                  mSchedulableMBeanArgumentTypes[i] = Date.class.getName();
               }
               else if (lToken.equals("REPETITIONS"))
               {
                  mSchedulableMBeanArguments[i] = REPETITIONS;
                  mSchedulableMBeanArgumentTypes[i] = Long.TYPE.getName();
               }
               else if (lToken.equals("SCHEDULER_NAME"))
               {
                  mSchedulableMBeanArguments[i] = SCHEDULER_NAME;
                  mSchedulableMBeanArgumentTypes[i] = ObjectName.class.getName();
               }
               else
               {
                  mSchedulableMBeanArguments[i] = NULL;
                  //AS ToDo: maybe later to check if this class exists !
                  mSchedulableMBeanArgumentTypes[i] = lToken;
               }
            }
         }
      }
      mSchedulableMBeanMethodName = lMethodName;
      mSchedulableMBeanMethod = pSchedulableMBeanMethod;
   }

   /**
    * @jmx:managed-attribute
    *
    * @return True if the Scheduler uses a Schedulable MBean, false if it uses a
    *         Schedulable class
    */
   public boolean isUsingMBean()
   {
      return mUseMBean;
   }

   /**
    * @jmx:managed-attribute
    *
    * @return Schedule Period between two scheduled calls in Milliseconds. It will always
    *         be bigger than 0 except it returns -1 then the schedule is stopped.
    */
   public long getSchedulePeriod()
   {
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
   public void setSchedulePeriod(long pPeriod)
   {
      if (pPeriod <= 0)
      {
         throw new InvalidParameterException("Schedulable Period may be not less or equals than 0");
      }
      mSchedulePeriod = pPeriod;
      mIsRestartPending = true;
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
   public String getInitialStartDate()
   {
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
   public void setInitialStartDate(String pStartDate)
   {
      mStartDateString = pStartDate == null ? "" : pStartDate.trim();
      if (mStartDateString.equals(""))
      {
         mStartDate = new Date(0);
      }
      else if (mStartDateString.equals("NOW"))
      {
         mStartDate = new Date(new Date().getTime() + 1000);
         mStartDateIsNow = true;
      }
      else
      {
         try
         {
            long lDate = new Long(pStartDate).longValue();
            mStartDate = new Date(lDate);
            mStartDateIsNow = false;
         }
         catch (Exception e)
         {
            try
            {
               if (mDateFormatter == null)
               {
                  mDateFormatter = new SimpleDateFormat();
               }
               mStartDate = mDateFormatter.parse(mStartDateString);
               mStartDateIsNow = false;
            }
            catch (Exception e2)
            {
               log.error("Could not parse given date string: " + mStartDateString, e2);
               throw new InvalidParameterException("Schedulable Date is not of correct format");
            }
         }
      }
      log.debug("Initial Start Date is set to: " + mStartDate);
   }

   /**
    * @jmx:managed-attribute
    *
    * @return Number of scheduled calls initially. If -1 then there is not limit.
    */
   public long getInitialRepetitions()
   {
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
   public void setInitialRepetitions(long pNumberOfCalls)
   {
      if (pNumberOfCalls <= 0)
      {
         pNumberOfCalls = -1;
      }
      mInitialRepetitions = pNumberOfCalls;
      mIsRestartPending = true;
   }

   /**
    * @jmx:managed-attribute
    *
    * @return Number of remaining repetitions. If -1 then there is no limit.
    */
   public long getRemainingRepetitions()
   {
      return mRemainingRepetitions;
   }

   /**
    * @jmx:managed-attribute
    *
    * @return True if the schedule is up and running. If you want to start the schedule
    *         with another values by using {@ #startSchedule} you have to stop the schedule
    *         first with {@ #stopSchedule} and wait until this method returns false.
    */
   public boolean isStarted()
   {
      return mScheduleIsStarted;
   }

   /**
    * @jmx:managed-attribute
    *
    * @return True if any attributes are changed but the Schedule is not restarted yet.
    */
   public boolean isRestartPending()
   {
      return mIsRestartPending;
   }

   /**
    * @jmx:managed-attribute
    *
    * @return True if the Schedule when the Scheduler is started
    */
   public boolean isStartAtStartup()
   {
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
   public void setStartAtStartup(boolean pStartAtStartup)
   {
      mStartOnStart = pStartAtStartup;
   }

   /**
    * @jmx:managed-attribute
    *
    * @return True if this Scheduler is active and will send notifications in the future
    */
   public boolean isActive()
   {
      return isStarted() && mRemainingRepetitions != 0;
   }

   /**
    * @jmx:managed-attribute
    *
    * @return Name of the Timer MBean used in here
    */
   public String getTimerName()
   {
      return mTimerName;
   }

   /**
    * @jmx:managed-attribute
    *
    * @param pTimerName Object Name of the Timer MBean to
    *                   be used. If null or not a valid ObjectName
    *                   the default will be used
    */
   public void setTimerName(String pTimerName)
   {
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

   protected void startService()
      throws Exception
   {
      // Create Timer MBean if need be

      try
      {
         mTimer = new ObjectName(mTimerName);
      }
      catch (MalformedObjectNameException mone)
      {
         mTimer = new ObjectName(DEFAULT_TIMER_NAME);
      }

      if (!getServer().isRegistered(mTimer))
      {
         getServer().createMBean("javax.management.timer.Timer", mTimer);
      }
      if (!((Boolean) getServer().getAttribute(mTimer, "Active")).booleanValue())
      {
         // Now start the Timer
         getServer().invoke(
            mTimer,
            "start",
            new Object[]{},
            new String[]{}
         );
      }

      if (mStartOnStart)
      {
         log.debug("Start Scheduler on start up time");
         startSchedule();
      }
   }

   protected void stopService()
   {
      // Stop the schedule right now !!
      stopSchedule(true);
   }

   // -------------------------------------------------------------------------
   // Inner Classes
   // -------------------------------------------------------------------------

   public class Listener
      implements NotificationListener
   {
      private final Logger log = Logger.getLogger(Listener.class);
      private Schedulable mDelegate;

      public Listener(Schedulable pDelegate)
      {
         mDelegate = pDelegate;
      }

      public void handleNotification(
         Notification pNotification,
         Object pHandback
         )
      {
         log.debug("Listener.handleNotification(), notification: " + pNotification);
         try
         {
            // If schedule is started invoke the schedule method on the Schedulable instance
            log.debug("Scheduler is started: " + isStarted());
            Date lTimeStamp = new Date(pNotification.getTimeStamp());
            if (isStarted())
            {
               if (getRemainingRepetitions() > 0 || getRemainingRepetitions() < 0)
               {
                  if (mRemainingRepetitions > 0)
                  {
                     mRemainingRepetitions--;
                  }
                  mDelegate.perform(
                     lTimeStamp,
                     getRemainingRepetitions()
                  );
                  log.debug("Remaining Repititions: " + getRemainingRepetitions() +
                     ", wait for next call to stop: " + mWaitForNextCallToStop);
                  if (getRemainingRepetitions() == 0 || mWaitForNextCallToStop)
                  {
                     stopSchedule(true);
                  }
               }
            }
            else
            {
               // Schedule is stopped therefore remove the Schedule
               getServer().invoke(
                  mTimer,
                  "removeNotification",
                  new Object[]{
                     new Integer(mActualSchedule)
                  },
                  new String[]{
                     Integer.class.getName()
                  }
               );
               mActualSchedule = -1;
            }
         }
         catch (Exception e)
         {
            log.error("Handling a Scheduler call failed", e);
         }
      }
   }

   public class MBeanListener
      implements NotificationListener
   {
      private final Logger log = Logger.getLogger(Listener.class);

      private ObjectName mDelegate;

      public MBeanListener(ObjectName pDelegate)
      {
         mDelegate = pDelegate;
      }

      public void handleNotification(
         Notification pNotification,
         Object pHandback
         )
      {
         log.debug("MBeanListener.handleNotification(), notification: " + pNotification);
         try
         {
            // If schedule is started invoke the schedule method on the Schedulable instance
            log.debug("Scheduler is started: " + isStarted());
            Date lTimeStamp = new Date(pNotification.getTimeStamp());
            if (isStarted())
            {
               if (getRemainingRepetitions() > 0 || getRemainingRepetitions() < 0)
               {
                  if (mRemainingRepetitions > 0)
                  {
                     mRemainingRepetitions--;
                  }
                  Object[] lArguments = new Object[mSchedulableMBeanArguments.length];
                  for (int i = 0; i < lArguments.length; i++)
                  {
                     switch (mSchedulableMBeanArguments[i])
                     {
                        case NOTIFICATION:
                           lArguments[i] = pNotification;
                           break;
                        case DATE:
                           lArguments[i] = lTimeStamp;
                           break;
                        case REPETITIONS:
                           lArguments[i] = new Long(mRemainingRepetitions);
                           break;
                        case SCHEDULER_NAME:
                           lArguments[i] = getServiceName();
                           break;
                        default:
                           lArguments[i] = null;
                     }
                  }
                  log.debug("MBean Arguments are: " + java.util.Arrays.asList(lArguments));
                  log.debug("MBean Arguments Types are: " + java.util.Arrays.asList(mSchedulableMBeanArgumentTypes));
                  try
                  {
                     getServer().invoke(
                        mDelegate,
                        mSchedulableMBeanMethodName,
                        lArguments,
                        mSchedulableMBeanArgumentTypes
                     );
                  }
                  catch (javax.management.JMRuntimeException jmre)
                  {
                     log.error("Invoke of the Schedulable MBean failed", jmre);
                  }
                  catch (javax.management.JMException jme)
                  {
                     log.error("Invoke of the Schedulable MBean failed", jme);
                  }
                  log.debug("Remaining Repititions: " + getRemainingRepetitions() +
                     ", wait for next call to stop: " + mWaitForNextCallToStop);
                  if (getRemainingRepetitions() == 0 || mWaitForNextCallToStop)
                  {
                     stopSchedule(true);
                  }
               }
            }
            else
            {
               // Schedule is stopped therefore remove the Schedule
               getServer().invoke(
                  mTimer,
                  "removeNotification",
                  new Object[]{
                     new Integer(mActualSchedule)
                  },
                  new String[]{
                     Integer.class.getName()
                  }
               );
               mActualSchedule = -1;
            }
         }
         catch (Exception e)
         {
            log.error("Handling a Scheduler call failed", e);
         }
      }
   }

   /**
    * Filter to ensure that each Scheduler only gets notified when it is supposed to.
    */
   private static class NotificationFilter implements javax.management.NotificationFilter
   {

      /** Class logger. */
      private static final Logger log = Logger.getLogger(NotificationFilter.class);

      private Integer mId;

      /**
       * Create a Filter.
       * @param id the Scheduler id
       */
      public NotificationFilter(Integer pId)
      {
         mId = pId;
      }

      /**
       * Determine if the notification should be sent to this Scheduler
       */
      public boolean isNotificationEnabled(Notification pNotification)
      {
         if (pNotification instanceof TimerNotification)
         {
            TimerNotification lTimerNotification = (TimerNotification) pNotification;
            if (log.isTraceEnabled())
               log.trace("Scheduler.NotificationFilter.isNotificationEnabled(), Id: " + mId +
                  ", notification: " + pNotification +
                  ", notification Id: " + lTimerNotification.getNotificationID() +
                  ", timestamp: " + lTimerNotification.getTimeStamp() +
                  ", message: " + lTimerNotification.getMessage()
               );
            return lTimerNotification.getNotificationID().equals(mId);
         }
         return false;
      }
   }

   /**
    * A test class for a Schedulable Class
    **/
   public static class SchedulableExample
      implements Schedulable
   {

      /** Class logger. */
      private static final Logger log = Logger.getLogger(Scheduler.SchedulableExample.class);

      private String mName;
      private int mValue;

      public SchedulableExample(
         String pName,
         int pValue
         )
      {
         mName = pName;
         mValue = pValue;
      }

      /**
       * Just log the call
       **/
      public void perform(
         Date pTimeOfCall,
         long pRemainingRepetitions
         )
      {
         log.info("Schedulable Examples is called at: " + pTimeOfCall +
            ", remaining repetitions: " + pRemainingRepetitions +
            ", test, name: " + mName + ", value: " + mValue);
      }
   }
}
