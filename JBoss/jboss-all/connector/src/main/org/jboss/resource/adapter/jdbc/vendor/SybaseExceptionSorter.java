package org.jboss.resource.adapter.jdbc.vendor;

import java.io.Serializable;
import java.sql.SQLException;

import org.jboss.resource.adapter.jdbc.ExceptionSorter;

/**
 * SybaseExceptionSorter.java
 *
 * Created: Wed May 12 11:46:23 2003
 *
 * @author <a href="mailto:corby3000 at hotmail.com">Corby Page</a>
 * @author <a href="mailto:an_test@mail.ru">Andrey Demchenko</a>
 * @author <a href="mailto:d_jencks@users.sourceforge.net">David Jencks</a>
 */
public class SybaseExceptionSorter implements ExceptionSorter, Serializable
{
    public boolean isExceptionFatal( SQLException e )
    {
        boolean result = false;

        String errorText = ( e.getMessage() ).toUpperCase();

        if ( ( errorText.indexOf( "JZ0C0" ) > -1 ) ||   // ERR_CONNECTION_DEAD
                ( errorText.indexOf( "JZ0C1" ) > -1 ) ) // ERR_IOE_KILLED_CONNECTION
        {
            result = true;
        }

        return result;
    }
}
