package org.jboss.resource.adapter.jdbc.vendor;

import java.io.Serializable;
import java.sql.SQLException;
import org.jboss.resource.adapter.jdbc.ExceptionSorter;


/**
 * OracleExceptionSorter.java
 *
 *
 * Created: Fri Mar 14 21:54:23 2003
 *
 * @author <a href="mailto:an_test@mail.ru">Andrey Demchenko</a>
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 * @version 1.0
 */
public class OracleExceptionSorter implements ExceptionSorter, Serializable
{
   public OracleExceptionSorter()
   {

   } // OracleExceptionSorter constructor

   public boolean isExceptionFatal(SQLException e)
   {

      String error_text = (e.getMessage()).toUpperCase();

      /* Check oracle specific errors for broadcasting connectionerror
      */
      return (error_text.indexOf("ORA-00600") > -1) //Internal oracle error
         || (error_text.indexOf("ORA-00028") > -1)  //session has been killed
         || (error_text.indexOf("ORA-01014") > -1)  //Oracle shutdown in progress
         || (error_text.indexOf("ORA-01033") > -1)  //Oracle initialization or shutdown in progress
         || (error_text.indexOf("ORA-01034") > -1)  //Oracle not available
         || (error_text.indexOf("ORA-03111") > -1)  //break received on communication channel
         || (error_text.indexOf("ORA-03113") > -1)  //end-of-file on communication channel
         || (error_text.indexOf("ORA-03114") > -1)  //not connected to ORACLE
         || (error_text.indexOf("TNS-") > -1)       //Net8 messages
         || (error_text.indexOf("SOCKET") > -1)     //for control socket error
         || (error_text.indexOf("BROKEN PIPE") > -1);

   }

} // OracleExceptionSorter
