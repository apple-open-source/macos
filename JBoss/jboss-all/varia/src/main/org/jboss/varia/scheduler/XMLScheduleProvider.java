/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.varia.scheduler;

import java.security.InvalidParameterException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Iterator;
import java.util.StringTokenizer;

import javax.management.JMException;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.w3c.dom.Text;

/**
 * This Provider get its Scheduler from a XML configuration string
 * allowing the administrator to add several Schedules with one
 * provider.
 * The "Schedules" properties has to look like:
 * <schedules>
 *    <schedule>
 *       <target-mbean-name/>
 *       <target-method-name/>
 *       <target-method-signature/>
 *       <start-date/>
 *       <period/>
 *       <repetitions/>
 *    </schedule>
 * </schedules>
 *
 * @jmx:mbean name="jboss:service=XMLScheduleProvider"
 *            extends="org.jboss.varia.scheduler.AbstractScheduleProviderMBean"
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.1 $
 */
public class XMLScheduleProvider
   extends AbstractScheduleProvider
   implements XMLScheduleProviderMBean
{

   // -------------------------------------------------------------------------
   // Constants
   // -------------------------------------------------------------------------
   
   // -------------------------------------------------------------------------
   // Members
   // -------------------------------------------------------------------------
   
   private Element mSchedules;
   
   /** The ID of the Schedule used later to remove it later **/
   private ArrayList mIDList = new ArrayList();
   
   // -------------------------------------------------------------------------
   // Constructors
   // -------------------------------------------------------------------------
   
   /**
    * Default (no-args) Constructor
    **/
   public XMLScheduleProvider()
   {
   }
   
   // -------------------------------------------------------------------------
   // SchedulerMBean Methods
   // -------------------------------------------------------------------------
   
   /**
    * @return XML configuration attribute
    *
    * @jmx:managed-operation
    **/
   public Element getSchedules() {
      return mSchedules;
   }
   
   /**
    * Sets the XML configuration attribute
    *
    * @jmx:managed-operation
    **/
   public void setSchedules( final Element pSchedules ) {
      mSchedules = pSchedules;
   }
   
   /**
    * Add the Schedule to the Schedule Manager
    *
    * @jmx:managed-operation
    **/
   public void startProviding()
      throws Exception
   {
      try {
      NodeList lSchedules = mSchedules.getElementsByTagName( "schedule" );
      for( int i = 0; i < lSchedules.getLength(); i++ ) {
         Node lSchedule = lSchedules.item( i );
         NodeList lAttributes = lSchedule.getChildNodes();
         Text lItem = getNode( lAttributes, "target-mbean-name" );
         if( lItem == null ) {
            log.error( "No 'target-mbean-name' is specified therefore this Schedule is ignored" );
            continue;
         }
         log.info( "Got 'target-mbean-name' element: " + lItem + ", node value: " + lItem.getData() + lItem.getChildNodes() );
         String lTarget = lItem.getData();
         lItem = getNode( lAttributes, "target-method-name" );
         if( lItem == null ) {
            log.error( "No 'target-method-name' is specified therefore this Schedule is ignored" );
            continue;
         }
         String lMethodName = lItem.getData();
         lItem = getNode( lAttributes, "target-method-signature" );
         if( lItem == null ) {
            log.error( "No 'target-method-signature' is specified therefore this Schedule is ignored" );
            continue;
         }
         String lMethodSignature = lItem.getData();
         lItem = getNode( lAttributes, "date-format" );
         String dateFormat = null;
         if (lItem != null)
         {
            dateFormat = lItem.getData();
            if (dateFormat != null && dateFormat.trim().length() != 0)
            try
            {
               new SimpleDateFormat(dateFormat);
            }
            catch (Exception e)
            {
              log.error( "Invalid date format therefore this Schedule is ignored", e);
              continue;
            }
         }
         lItem = getNode( lAttributes, "start-date" );
         if( lItem == null ) {
            log.error( "No 'start-date' is specified therefore this Schedule is ignored" );
            continue;
         }
         String lStartDate = lItem.getData();
         lItem = getNode( lAttributes, "period" );
         if( lItem == null ) {
            log.error( "No 'period' is specified therefore this Schedule is ignored" );
            continue;
         }
         String lPeriod = lItem.getData();
         lItem = getNode( lAttributes, "repetitions" );
         if( lItem == null ) {
            log.error( "No 'repetitions' is specified therefore this Schedule is ignored" );
            continue;
         }
         String lRepeptions = lItem.getData();
         try {
            // Add Schedule
            int lID = addSchedule(
               new ObjectName( lTarget ),
               lMethodName,
               getSignature( lMethodSignature ),
               getStartDate( lStartDate, dateFormat ),
               new Long( lPeriod ).longValue(),
               new Integer( lRepeptions ).intValue()
            );
            mIDList.add( new Integer( lID ) );
         }
         catch( NumberFormatException nfe ) {
            log.error( "Could not convert a number", nfe );
         }
      }
      } catch( Exception e ) {
         e.printStackTrace();
         throw e;
      }
   }
   
   /**
    * Loops of the given Node List and looks for the Element
    * with the given Node Name
    *
    * @param pNodeList The list of nodes to search through
    * @param pName The name of the node to look for
    *
    * @return Element if found otherwise null
    **/
   protected Text getNode( NodeList pList, String pName ) {
      if( pList == null ) {
         return null;
      }
      for( int i = 0; i < pList.getLength(); i++ ) {
         Node lNode = pList.item(i);
         switch( lNode.getNodeType() ) {
            case Node.ELEMENT_NODE:
               Element lChild = (Element) lNode;
               if( lChild.getNodeName().equals( pName ) ) {
                  return (Text) lChild.getFirstChild();
               }
         }
      }
      return null;
   }
   
   /**
    * Stops the Provider from providing causing
    * the provider to remove the Schedule
    *
    * @jmx:managed-operation
    */
   public void stopProviding() {
      Iterator i = mIDList.iterator();
      while( i.hasNext() ) {
         Integer lID = (Integer) i.next();
         try {
            removeSchedule( lID.intValue() );
         }
         catch( JMException jme ) {
            log.error( "Could not remove Schedule in stop providing", jme );
         }
      }
   }
   
   /**
    * Converts a string of method arguments (separated by colons) into
    * an array of string
    **/
   protected String[] getSignature( String pMethodSignature )
   {
      if( pMethodSignature == null || "".equals( pMethodSignature.trim() ) ) {
         return new String[ 0 ];
      }
      StringTokenizer lTokenizer = new StringTokenizer( pMethodSignature, "," );
      String[] lReturn = new String[ lTokenizer.countTokens() ];
      int i = 0;
      while( lTokenizer.hasMoreTokens() ) {
         lReturn[ i++ ] = lTokenizer.nextToken().trim();
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
   protected Date getStartDate( String pStartDate, String dateFormat ) {
      pStartDate = pStartDate == null ? "" : pStartDate.trim();
      Date lReturn = null;
      if( pStartDate.equals( "" ) ) {
         lReturn = new Date( 0 );
      } else
      if( pStartDate.equals( "NOW" ) ) {
         lReturn = new Date( new Date().getTime() + 1000 );
      } else {
         try {
            long lDate = new Long( pStartDate ).longValue();
            lReturn = new Date( lDate );
         }
         catch( Exception e ) {
            try {
               SimpleDateFormat dateFormatter = null;
               if( dateFormat == null || dateFormat.trim().length() == 0 )
                  dateFormatter = new SimpleDateFormat();
               else
                  dateFormatter = new SimpleDateFormat(dateFormat);
               lReturn = dateFormatter.parse( pStartDate );
            }
            catch( Exception e2 ) {
               log.error( "Could not parse given date string: " + pStartDate, e2 );
               throw new InvalidParameterException( "Schedulable Date is not of correct format" );
            }
         }
      }
      log.debug( "Initial Start Date is set to: " + lReturn );
      
      return lReturn;
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
      return pName == null ? XMLScheduleProviderMBean.OBJECT_NAME : pName;
   }
}
