package org.jboss.util;

import java.util.ArrayList;

/**
 * Created by IntelliJ IDEA.
 * User: wburke
 * Date: Oct 1, 2003
 * Time: 3:29:04 PM
 * To change this template use Options | File Templates.
 */
public class ProfilerPoint
{
   public final String point;
   public final long time = System.currentTimeMillis();

   public ProfilerPoint(String p)
   {
      point = p;

   }
   private static final ThreadLocal profilePoints = new ThreadLocal();
   private static final ThreadLocal startTime = new ThreadLocal();

   public static void start()
   {
      startTime.set(new Long(System.currentTimeMillis()));
   }

   public static long getStartTime()
   {
      Long l = (Long)startTime.get();
      if (l == null) return -1;
      return l.longValue();
   }

   public static void push(String point)
   {
      ArrayList points = (ArrayList)profilePoints.get();
      if (points == null)
      {
         points = new ArrayList(20);
         profilePoints.set(points);
      }
      points.add(new ProfilerPoint(point));
   }
   public static void pop()
   {
      ArrayList points = (ArrayList)profilePoints.get();
      if (points == null) return;
      if (points.size() == 0) return;
      points.remove(points.size() - 1);
   }
   public static ArrayList points()
   {
      return (ArrayList)profilePoints.get();
   }

   public static void clear()
   {
      profilePoints.set(null);
   }

   private static final String NOW = "NOW ";
   public static String stack()
   {
      ArrayList points = (ArrayList)profilePoints.get();
      if (points == null) return NOW;
      StringBuffer buf = new StringBuffer();
      for (int i = 0; i < points.size(); ++i)
      {
         ProfilerPoint point = (ProfilerPoint)points.get(i);
         buf.append(point.point + " " + point.time);
         buf.append(":");
      }
      buf.append(NOW + System.currentTimeMillis());
      return buf.toString();
   }
}
