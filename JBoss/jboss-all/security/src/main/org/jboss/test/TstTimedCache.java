package org.jboss.test;

import org.jboss.util.TimedCachePolicy;

/** Tests of the TimedCachePolicy class.

@see org.jboss.util.TimedCachePolicy

@author Scott.Stark@jboss.org
@version $Revision: 1.4.2.1 $
*/
public class TstTimedCache
{
    static class Refreshable implements TimedCachePolicy.TimedEntry
    {
        int refreshes;
        long expirationTime;
        Object value;
        Refreshable(long lifetime, Object value, int refreshes)
        {
            this.expirationTime = 1000 * lifetime;
            this.value = value;
            this.refreshes = refreshes;
        }
        public void init(long now)
        {
            expirationTime += now;
            System.out.println(value+".init("+now+"), expirationTime="+expirationTime);
        }
        public boolean isCurrent(long now)
        {
            System.out.println(value+".isCurrent("+now+") = "+(expirationTime > now));
            return expirationTime > now;
        }
        public boolean refresh()
        {
            refreshes --;
            System.out.println(value+".refresh() = "+(refreshes > 0));
            return refreshes > 0;
        }
        public void destroy()
        {
            System.out.println(value+".destroy()");
        }
        public Object getValue()
        {
            return value;
        }
    }

    /**
    * @param args the command line arguments
    */
    public static void main(String args[])
    {
        TimedCachePolicy cache = new TimedCachePolicy(20, false, 1);
        cache.create();
        cache.start();
        cache.insert("1", new Refreshable(5, "value1", 4));
        cache.insert("2", new Refreshable(3, "value2", 10));
        cache.insert("3", "value3");
        long start = System.currentTimeMillis();
        // Loop until the longest lived value is gone
        while( cache.peek("2") != null )
        {
            long now = System.currentTimeMillis();
            System.out.println("Elapsed: "+(now - start) / 1000);
            System.out.println("get(1) -> "+cache.get("1"));
            System.out.println("get(2) -> "+cache.get("2"));
            System.out.println("get(3) -> "+cache.get("3"));
            try
            {
                Thread.sleep(3 * 1000);
            }
            catch(InterruptedException e)
            {
            }
        }
        long now = System.currentTimeMillis();
        System.out.println("End, elapsed: "+(now - start) / 1000);
        System.out.println("get(1) -> "+cache.get("1"));
        System.out.println("get(2) -> "+cache.get("2"));
        System.out.println("get(3) -> "+cache.get("3"));
    }

}
