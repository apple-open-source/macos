/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.console.manager.interfaces.impl;

import org.jboss.console.navtree.AppletBrowser;
import org.jboss.console.navtree.AppletTreeAction;
import org.jboss.console.navtree.TreeContext;
import org.jfree.chart.ChartFactory;
import org.jfree.chart.ChartFrame;
import org.jfree.chart.JFreeChart;
import org.jfree.chart.plot.PlotOrientation;
import org.jfree.data.AbstractSeriesDataset;
import org.jfree.data.XYDataset;
import org.jfree.data.DatasetChangeEvent;

import javax.management.ObjectName;
import java.util.ArrayList;

/**
 * <description>
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.1.2.1 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>3 janv. 2003 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */
public class GraphMBeanAttributeAction
        implements AppletTreeAction
{
   public class MBeanXYDataset extends AbstractSeriesDataset implements XYDataset
   {

      private ArrayList data = new ArrayList();

      /**
       * Default constructor.
       */
      public MBeanXYDataset()
      {
      }

      public void clear()
      {
         data.clear();
         notifyListeners(new DatasetChangeEvent(this, this));
      }

      public void add(Object num)
      {
         data.add(num);
         notifyListeners(new DatasetChangeEvent(this, this));
      }

      /**
       * Returns the x-value for the specified series and item.  Series are numbered 0, 1, ...
       *
       * @param series  the index (zero-based) of the series.
       * @param item  the index (zero-based) of the required item.
       *
       * @return the x-value for the specified series and item.
       */
      public Number getXValue(int series, int item)
      {
         return new Integer(item);
      }

      /**
       * Returns the y-value for the specified series and item.  Series are numbered 0, 1, ...
       *
       * @param series  the index (zero-based) of the series.
       * @param item  the index (zero-based) of the required item.
       *
       * @return the y-value for the specified series and item.
       */
      public Number getYValue(int series, int item)
      {
         return (Number) data.get(item);
      }

      /**
       * Returns the number of series in the dataset.
       *
       * @return the number of series in the dataset.
       */
      public int getSeriesCount()
      {
         return 1;
      }

      /**
       * Returns the name of the series.
       *
       * @param series  the index (zero-based) of the series.
       *
       * @return the name of the series.
       */
      public String getSeriesName(int series)
      {
         return "y = " + attr;
      }

      /**
       * Returns the number of items in the specified series.
       *
       * @param series  the index (zero-based) of the series.
       * @return the number of items in the specified series.
       *
       */
      public int getItemCount(int series)
      {
         return data.size();
      }
   }

   public class UpdateThread implements Runnable
   {
      MBeanXYDataset data;
      TreeContext tc;

      public UpdateThread(MBeanXYDataset data, TreeContext tc)
      {
         this.data = data;
         this.tc = tc;
      }

      public void run()
      {
         while (true)
         {
            try
            {
               if (frame.isShowing())
               {
                  Object val = tc.getRemoteMBeanInvoker().getAttribute(targetObjectName, attr);
                  System.out.println("added value: " + val);
                  data.add(val);
               }
               Thread.sleep(1000);
            }
            catch (Exception ex)
            {
               ex.printStackTrace();
            }
         }
      }
   }


   protected ObjectName targetObjectName = null;
   protected String attr = null;
   protected transient ChartFrame frame = null;
   protected transient MBeanXYDataset dataset = null;

   public GraphMBeanAttributeAction()
   {
   }

   public GraphMBeanAttributeAction(ObjectName pName,
                                    String attr)
   {
      this.targetObjectName = pName;
      this.attr = attr;
   }

   public void doAction(TreeContext tc, AppletBrowser applet)
   {
      try
      {
         if (frame == null)
         {
            //tc.getRemoteMBeanInvoker ().invoke(targetObjectName, actionName, params, signature);
            dataset = new MBeanXYDataset();
            JFreeChart chart = ChartFactory.createXYLineChart(
                    "JMX Attribute: " + attr, "count", attr, dataset,
                    PlotOrientation.VERTICAL,
                    true,
                    true,
                    false
            );
            UpdateThread update = new UpdateThread(dataset, tc);

            Thread thread = new Thread(update);
            thread.start();
            frame = new ChartFrame("JMX Attribute: " + attr, chart);
            frame.getChartPanel().setPreferredSize(new java.awt.Dimension(500, 270));
            frame.pack();
         }
         else
         {
            dataset.clear();
         }
         frame.show();
         frame.requestFocus();
      }
      catch (Exception displayed)
      {
         displayed.printStackTrace();
      }
   }

}
