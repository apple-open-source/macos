/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.test.xa.bean;

import java.sql.Connection;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;
import javax.ejb.CreateException;
import javax.ejb.EJBException;
import javax.ejb.SessionBean;
import javax.ejb.SessionContext;
import javax.naming.Context;
import javax.naming.InitialContext;
import javax.naming.NamingException;
import javax.sql.DataSource;
import org.jboss.test.xa.interfaces.CantSeeDataException;

public class XATestBean
    implements SessionBean
{
   org.apache.log4j.Category log = org.apache.log4j.Category.getInstance(getClass());
   
    public final static String DROP_TABLE =
        "DROP TABLE XA_TEST";

    public final static String CREATE_TABLE =
        "CREATE TABLE XA_TEST(ID INTEGER NOT NULL PRIMARY KEY, DATA INTEGER NOT NULL)";

    public final static String DB_1_NAME = "java:comp/env/jdbc/DBConnection1";
    public final static String DB_2_NAME = "java:comp/env/jdbc/DBConnection2";

    public XATestBean() {
    }

    public void ejbCreate() throws CreateException {
    }

    public void ejbActivate() throws EJBException {
    }

    public void ejbPassivate() throws EJBException {
    }

    public void ejbRemove() throws EJBException {
    }

    public void setSessionContext(SessionContext parm1) throws EJBException {
    }

    protected void execute(DataSource ds, String sql) throws SQLException {
        Connection con = ds.getConnection();
        try {
            Statement s = con.createStatement();
            s.execute(sql);
            s.close();
        }
        finally {
            con.close();
        }
    }

    protected void execute(Connection con, String sql) throws SQLException {
        Statement s = con.createStatement();
        s.execute(sql);
        s.close();
    }
    
    public void createTables() throws NamingException, SQLException {
        Context ctx = new InitialContext();
        try {
            DataSource ds1 = (DataSource)ctx.lookup(DB_1_NAME);
            try {
                execute(ds1, DROP_TABLE);
            }
            catch (Exception ignore) {}
            execute(ds1, CREATE_TABLE);

            DataSource ds2 = (DataSource)ctx.lookup(DB_2_NAME);
            try {
                execute(ds2, DROP_TABLE);
            }
            catch (Exception ignore) {}
            execute(ds2, CREATE_TABLE);
        }
        finally {
            ctx.close();
        }
    }
    
    public void clearData() {
        try {
            Context ctx = new InitialContext();
            DataSource db1ds = (DataSource)ctx.lookup(DB_1_NAME);
            Connection db1con = db1ds.getConnection();
            Statement db1st = db1con.createStatement();
            db1st.executeUpdate("DELETE FROM XA_TEST");
            db1st.close();

            DataSource db2ds = (DataSource)ctx.lookup(DB_2_NAME);
            Connection db2con = db2ds.getConnection();
            Statement db2st = db2con.createStatement();
            db2st.executeUpdate("DELETE FROM XA_TEST");
            db2st.close();

            db2con.close();
            db1con.close();
        } catch(SQLException e) {
            throw new EJBException("Unable to clear data (have tables been created?): "+e);
        } catch(NamingException e) {
            throw new EJBException("Unable to find DB pool: "+e);
        }
    }

    public void doWork() throws CantSeeDataException {
        Connection db1cona = null, db1conb = null, db2con = null;
        try {
        // Create 3 connections
            Context ctx = new InitialContext();
            DataSource db1ds = (DataSource)ctx.lookup(DB_1_NAME);
            db1cona = db1ds.getConnection();
            db1conb = db1ds.getConnection();
            DataSource db2ds = (DataSource)ctx.lookup(DB_2_NAME);
            db2con = db2ds.getConnection();

        // Insert some data on one connection
            Statement s = db1cona.createStatement();
            int data = (int)(System.currentTimeMillis() & 0x0000FFFFL);
            s.executeUpdate("INSERT INTO XA_TEST (ID, DATA) VALUES (1, "+data+")");
            s.close();

        // Verify that another connection on the same DS can read it
            s = db1conb.createStatement();
            int result = -1;
            ResultSet rs = s.executeQuery("SELECT DATA FROM XA_TEST WHERE ID=1");
            while(rs.next()) {
                result = rs.getInt(1);
            }
            rs.close();
            s.close();

        // Do some work on the other data source
            s = db2con.createStatement();
            s.executeUpdate("INSERT INTO XA_TEST (ID, DATA) VALUES (1, "+data+")");
            s.close();

            if(result != data)
                throw new CantSeeDataException("Insert performed on one connection wasn't visible\n"+
                                               "to another connection in the same transaction!");

        } catch(SQLException e) {
            throw new EJBException("Unable to clear data (have tables been created?): "+e);
        } catch(NamingException e) {
            throw new EJBException("Unable to find DB pool: "+e);
        } finally {
        // Close all connections
            if(db2con != null) try {db2con.close();}catch(SQLException e) {}
            if(db1cona != null) try {db1cona.close();}catch(SQLException e) {}
            if(db1conb != null) try {db1conb.close();}catch(SQLException e) {}
        }
    }
}
