import java.rmi.*;
import java.awt.*;
import java.util.*;
import javax.ejb.*;
import javax.naming.*;
import java.awt.event.*;
import java.util.*;
import java.lang.*;
import java.io.*;

public class jnditester
{
    public static void main(String[] args)
    {
        try
        {
            Properties p = new Properties();
            
            p.put(Context.INITIAL_CONTEXT_FACTORY, 
                  "org.jnp.interfaces.NamingContextFactory");
            p.put(Context.PROVIDER_URL, "10.10.10.13:1100,10.10.10.14:1100");
            //p.put(Context.PROVIDER_URL, "localhost:1100");
            p.put(Context.URL_PKG_PREFIXES, "org.jboss.naming:org.jnp.interfaces");
            InitialContext ctx = new InitialContext(p);
            String hello = "hello world";

            try
            {
                String found = (String)ctx.lookup("testit");
            }
            catch (NameNotFoundException nfe)
            {
                System.out.println("creating testit!!!");
                ctx.bind("testit", hello);
            }

            while (true)
            {
                String found = null;
                try
                {
                    found = (String)ctx.lookup("testit");
                    System.out.println("found: " + found);
                }
                catch (NameNotFoundException nfe)
                {
                    System.err.println("could not find testit");
                }
                Thread.sleep(2000);
            }
        }
        catch (Exception ex)
        {
            ex.printStackTrace();
        }
    }
}
