import java.rmi.*;
import java.awt.*;
import java.util.*;
import javax.ejb.*;
import javax.naming.*;
import java.awt.event.*;
import java.util.*;
import java.lang.*;
import java.io.*;
import org.jboss.test.testbean.interfaces.StatelessSessionHome;
import org.jboss.test.testbean.interfaces.StatelessSession;
import org.jboss.test.testbean.interfaces.EnterpriseEntityHome;
import org.jboss.test.testbean.interfaces.EnterpriseEntity;
import javax.ejb.DuplicateKeyException;
import javax.ejb.Handle;
import javax.ejb.EJBMetaData;
import javax.ejb.EJBHome;
import javax.ejb.HomeHandle;

public class slsb
{
    public static void main(String[] args)
    {
        try
        {
            Properties p = new Properties();
            
            p.put(Context.INITIAL_CONTEXT_FACTORY, 
                  "org.jnp.interfaces.NamingContextFactory");
            p.put(Context.PROVIDER_URL, "10.10.10.13:1100,10.10.10.14:1100");
            // p.put(Context.PROVIDER_URL, "localhost:1100");
            p.put(Context.URL_PKG_PREFIXES, "org.jboss.naming:org.jnp.interfaces");
            InitialContext ctx = new InitialContext(p);
            
            StatelessSessionHome  statelessSessionHome =  (StatelessSessionHome) ctx.lookup("nextgen.StatelessSession");
            EnterpriseEntityHome  cmpHome =  (EnterpriseEntityHome)ctx.lookup("nextgen.EnterpriseEntity");
            StatelessSession statelessSession = statelessSessionHome.create();
            EnterpriseEntity cmp = null;
            try
            {
               cmp = cmpHome.findByPrimaryKey("bill");
            }
            catch (Exception ex)
            {
               cmp = cmpHome.create("bill");
            }
            int count = 0;
            while (true)
            {
               System.out.println(statelessSession.callBusinessMethodB());
               try
               {
                  cmp.setOtherField(count++);
               }
               catch (Exception ex)
               {
                  System.out.println("exception, trying to create it: " + ex);
                  cmp = cmpHome.create("bill");
                  cmp.setOtherField(count++);
               }
               System.out.println("Entity: " + cmp.getOtherField());
               Thread.sleep(2000);
            }
        }
        catch (NamingException nex)
        {
           if (nex.getRootCause() != null)
           {
              nex.getRootCause().printStackTrace();
           }
        }
        catch (Exception ex)
        {
            ex.printStackTrace();
        }
    }
}
