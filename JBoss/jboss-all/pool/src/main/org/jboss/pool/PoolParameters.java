/*
 * Licensed under the X license (see http://www.x.org/terms.htm)
 */
package org.jboss.pool;

import java.io.Serializable;
import java.util.Properties;

/**
 *  Convenience wrapper for all ObjectPool parameters. See the ObjectPool
 *  setters and getters for descriptions of the parameters.
 *
 * @author     Aaron Mulder ammulder@alumni.princeton.edu
 * @created    August 18, 2001
 * @see        org.jboss.pool.ObjectPool
 */
public class PoolParameters implements Serializable {

   public int       minSize = 0;
   public int       maxSize = 0;
   public boolean   blocking = true;
    public long blockingTimeoutMillis = -1; //never times out on blocking
   public boolean   gcEnabled = false;
   public boolean   idleTimeoutEnabled = false;
   public boolean   invalidateOnError = false;
   public boolean   trackLastUsed = false;
   public long      cleanupIntervalMillis = 120000l;
   public long      gcMinIdleMillis = 1200000l;
   public long      idleTimeoutMillis = 1800000l;
   public float     maxIdleTimeoutPercent = 1.0f;
   public final static String MIN_SIZE_KEY = "MinSize";
   public final static String MAX_SIZE_KEY = "MaxSize";
   public final static String BLOCKING_KEY = "Blocking";
   public final static String BLOCKING_TIMEOUT_MS_KEY = "BlockingTimeoutMillis";
   public final static String CLEANUP_ENABLED_KEY = "CleanupEnabled";
   public final static String IDLE_TIMEOUT_ENABLED_KEY = "IdleTimeoutEnabled";
   public final static String INVALIDATE_ON_ERROR_KEY = "InvalidateOnError";
   public final static String TRACK_LAST_USED_KEY = "TimestampUsed";
   public final static String CLEANUP_INTERVAL_MS_KEY = "CleanupIntervalMillis";
   public final static String GC_MIN_IDLE_MS_KEY = "GCMinIdleMillis";
   public final static String IDLE_TIMEOUT_MS_KEY = "IdleTimeoutMillis";
   public final static String MAX_IDLE_TIMEOUT_PERCENT_KEY = "MaxIdleTimeoutPercent";

   public final static String CLEANUP_INTERVAL_MIN_KEY = "CleanupIntervalMinutes";
   public final static String IDLE_TIMEOUT_MIN_KEY = "IdleTimeoutMinutes";

   public PoolParameters(Properties props) {

      String s = props.getProperty(BLOCKING_TIMEOUT_MS_KEY);
      if (s != null)
      {
         try
         {
            blockingTimeoutMillis = Long.parseLong(s);
         }
         catch (Exception e)
         {
         }
      }
      /*
      s = props.getProperty(GC_ENABLED_KEY);
      if (s != null)
      {
         try
         {
            gcEnabled = new Boolean(s).booleanValue();
         }
         catch (Exception e)
         {
         }
         }*/
      s = props.getProperty(CLEANUP_INTERVAL_MIN_KEY);
      if (s != null)
      {
         try
         {
            setCleanupIntervalMinutes(Long.parseLong(s));
         }
         catch (Exception e)
         {
         }
      }
      /*
      s = props.getProperty(GC_MIN_IDLE_MS_KEY);
      if (s != null)
      {
         try
         {
            gcMinIdleMillis = Long.parseLong(s);
         }
         catch (Exception e)
         {
         }
         }*/
      /*
      s = props.getProperty(IDLE_TIMEOUT_ENABLED_KEY);
      if (s != null)
      {
         try
         {
            idleTimeoutEnabled = new Boolean(s).booleanValue();
         }
         catch (Exception e)
         {
         }
         }*/
      s = props.getProperty(IDLE_TIMEOUT_MIN_KEY);
      if (s != null)
      {
         try
         {
           setIdleTimeoutMinutes(Long.parseLong(s));
         }
         catch (Exception e)
         {
         }
      }
      /*
      s = props.getProperty(INVALIDATE_ON_ERROR_KEY);
      if (s != null)
      {
         try
         {
            invalidateOnError = new Boolean(s).booleanValue();
         }
         catch (Exception e)
         {
         }
         }*/
      s = props.getProperty(MAX_IDLE_TIMEOUT_PERCENT_KEY);
      if (s != null)
      {
         try
         {
            maxIdleTimeoutPercent = new Float(s).floatValue();
         }
         catch (Exception e)
         {
         }
      }
      s = props.getProperty(MAX_SIZE_KEY);
      if (s != null)
      {
         try
         {
            maxSize = Integer.parseInt(s);
         }
         catch (Exception e)
         {
         }
      }
      s = props.getProperty(MIN_SIZE_KEY);
      if (s != null)
      {
         try
         {
            minSize = Integer.parseInt(s);
         }
         catch (Exception e)
         {
         }
      }


   }
   public void setCleanupIntervalMinutes(long min)
   {
      cleanupIntervalMillis = min * 60 * 1000;
   }
   public void setIdleTimeoutMinutes(long min)
   {
      idleTimeoutMillis = min * 60 * 1000;
   }
}
