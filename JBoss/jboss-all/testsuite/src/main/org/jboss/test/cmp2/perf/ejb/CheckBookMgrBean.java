package org.jboss.test.cmp2.perf.ejb;

import java.util.Iterator;
import java.util.Calendar;
import java.util.Collection;
import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.FinderException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.InitialContext;
import javax.naming.Context;

import org.jboss.test.cmp2.perf.interfaces.LocalCheckBook;
import org.jboss.test.cmp2.perf.interfaces.LocalCheckBookEntry;
import org.jboss.test.cmp2.perf.interfaces.LocalCheckBookHome;
import org.jboss.test.cmp2.perf.interfaces.LocalCheckBookEntryHome;
import org.jboss.logging.Logger;

/**
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public class CheckBookMgrBean implements SessionBean
{
   private static Logger log = Logger.getLogger(CheckBookMgrBean.class);
   private LocalCheckBook checkBook;

   public CheckBookMgrBean()
   {
   }

   public void ejbCreate(String account, double balance) throws CreateException
   {
      try
      {
         InitialContext ctx = new InitialContext();
         Context enc = (Context) ctx.lookup("java:comp/env");
         LocalCheckBookHome home = (LocalCheckBookHome) enc.lookup("ejb/LocalCheckBookHome");
         try
         {
            checkBook = home.findByPrimaryKey(account);
         }
         catch(FinderException e)
         {
            log.info("Failed to find CheckBook for: "+account);
            checkBook = home.create(account, balance);
            // Populate the check book
            LocalCheckBookEntryHome home2 = (LocalCheckBookEntryHome) enc.lookup("ejb/LocalCheckBookEntryHome");
            populateCheckBook(home2);
         }
      }
      catch(Exception e)
      {
         log.error("Failed to setup CheckBookMgrBean", e);
         throw new CreateException("Failed to setup CheckBookMgrBean: "+e.getMessage());
      }
   }

   public void ejbActivate() throws EJBException
   {
   }

   public void ejbPassivate() throws EJBException
   {
   }

   public void ejbRemove() throws EJBException
   {
   }

   public void setSessionContext(SessionContext ctx) throws EJBException
   {
   }

   public int getEntryCount()
   {
      log.info("Begin getEntryCount");
      Collection entries = checkBook.getCheckBookEntries();
      int size = entries.size();
      log.info("End getEntryCount");
      return size;
   }
   public double getBalance()
   {
      log.info("Begin getBalance");
      double total = checkBook.getBalance();

      Iterator entries = checkBook.getCheckBookEntries().iterator();
      while (entries.hasNext())
      {
          LocalCheckBookEntry entry = (LocalCheckBookEntry) entries.next();
          total -= entry.getAmount();
      }
      log.info("End getBalance");

      return total;
   }
   public double entryTotalByCategory(String category)
   {
      double total = 0;
      return total;
   }
   public double[] entryTotalByMonth(int year)
   {
      double[] months = new double[12];
      return months;
   }

   public StringBuffer createAnnualReport(int year)
   {
      StringBuffer report = new StringBuffer();
      return report;
   }

   private void populateCheckBook(LocalCheckBookEntryHome home)
      throws CreateException
   {
      Calendar cal = Calendar.getInstance();
      Collection entries = checkBook.getCheckBookEntries();
      String[] categories = {"Business", "Personal", "Travel", "Expenses", "Misc"};
      int entryNo = 0;
      for(int month = Calendar.JANUARY; month <= Calendar.DECEMBER; month ++)
      {
         cal.set(2003, month, 1);
         int lastDay = cal.getActualMaximum(Calendar.DAY_OF_MONTH);
         for(int day = 2; day < lastDay; day ++)
         {
            long timestamp = cal.getTime().getTime();
            for(int n = 0; n < categories.length; n ++)
            {
               LocalCheckBookEntry entry = home.create(new Integer(entryNo));
               entryNo ++;
               entry.setAmount(1);
               entry.setTimestamp(timestamp);
               entry.setCategory(categories[n]);
               entries.add(entry);
               timestamp += 3600 * 1000;
            }
            cal.set(2003, month, day);
         }
      }
   }
}
