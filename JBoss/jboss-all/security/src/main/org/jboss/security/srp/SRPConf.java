/*
 * JBoss, the OpenSource J2EE WebOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.security.srp;

import java.math.BigInteger;

import org.jboss.security.Util;

/** A port of the libsrp/t_conf.c predefined constants for the N & g parameters
of the SRP algorithm. It contains a collection of "good" primes for N and the
corresponding the corresponding generator g.

This product includes software developed by Tom Wu and Eugene
Jhong for the SRP Distribution (http://srp.stanford.edu/srp/).

@author Scott.Stark@jboss.org
@version $Revision: 1.4 $
*/
public class SRPConf
{
    /* Master builtin parameter storage object */
    public static class SRPParams
    {
        String modb64;
        String genb64;
        String comment;
        BigInteger N;
        BigInteger g;
        SRPParams(String modb64, String genb64, String comment)
        {
            this.modb64 = modb64;
            this.genb64 = genb64;
            this.comment = comment;
        }
        public byte[] Nbytes()
        {
            return Util.fromb64(modb64);
        }
        public byte[] gbytes()
        {
            return Util.fromb64(genb64);
        }
        public BigInteger N()
        {
            if( N == null )
                N = new BigInteger(1, Util.fromb64(modb64));
            return N;
        }
        public BigInteger g()
        {
            if( g == null )
                g = new BigInteger(1, Util.fromb64(genb64));
            return g;
        }
        public String getComment()
        {
            return comment;
        }
    }

    static SRPParams[] pre_params = {
        new SRPParams("3Kn/YYiomHkFkfM1x4kayR125MGkzpLUDy3y14FlTMwYnhZkjrMXnoC2TcFAecNlU5kFzgcpKYUbBOPZFRtyf3",
            "2", null),
        new SRPParams("CbDP.jR6YD6wAj2ByQWxQxQZ7.9J9xkn2.Uqb3zVm16vQyizprhBw9hi80psatZ8k54vwZfiIeEHZVsDnyqeWSSIpWso.wh5GD4OFgdhVI3",
            "2", null),
        new SRPParams("iqJ7nFZ4bGCRjE1F.FXEwL085Zb0kLM2TdHDaVVCdq0cKxvnH/0FLskJTKlDtt6sDl89dc//aEULTVFGtcbA/tDzc.bnFE.DWthQOu2n2JwKjgKfgCR2lZFWXdnWmoOh",
            "2", null),
        new SRPParams("///////////93zgY8MZ2DCJ6Oek0t1pHAG9E28fdp7G22xwcEnER8b5A27cED0JTxvKPiyqwGnimAmfjybyKDq/XDMrjKS95v8MrTc9UViRqJ4BffZes8F//////////",
            "7", "oakley prime 1"),
        new SRPParams("Ewl2hcjiutMd3Fu2lgFnUXWSc67TVyy2vwYCKoS9MLsrdJVT9RgWTCuEqWJrfB6uE3LsE9GkOlaZabS7M29sj5TnzUqOLJMjiwEzArfiLr9WbMRANlF68N5AVLcPWvNx6Zjl3m5Scp0BzJBz9TkgfhzKJZ.WtP3Mv/67I/0wmRZ",
            "2", null),
        new SRPParams("F//////////oG/QeY5emZJ4ncABWDmSqIa2JWYAPynq0Wk.fZiJco9HIWXvZZG4tU.L6RFDEaCRC2iARV9V53TFuJLjRL72HUI5jNPYNdx6z4n2wQOtxMiB/rosz0QtxUuuQ/jQYP.bhfya4NnB7.P9A6PHxEPJWV//////////",
            "5", "oakley prime 2"),
        new SRPParams("3NUKQ2Re4P5BEK0TLg2dX3gETNNNECPoe92h4OVMaDn3Xo/0QdjgG/EvM.hiVV1BdIGklSI14HA38Mpe5k04juR5/EXMU0r1WtsLhNXwKBlf2zEfoOh0zVmDvqInpU695f29Iy7sNW3U5RIogcs740oUp2Kdv5wuITwnIx84cnO.e467/IV1lPnvMCr0pd1dgS0a.RV5eBJr03Q65Xy61R",
            "2", null),
        new SRPParams("dUyyhxav9tgnyIg65wHxkzkb7VIPh4o0lkwfOKiPp4rVJrzLRYVBtb76gKlaO7ef5LYGEw3G.4E0jbMxcYBetDy2YdpiP/3GWJInoBbvYHIRO9uBuxgsFKTKWu7RnR7yTau/IrFTdQ4LY/q.AvoCzMxV0PKvD9Odso/LFIItn8PbTov3VMn/ZEH2SqhtpBUkWtmcIkEflhX/YY/fkBKfBbe27/zUaKUUZEUYZ2H2nlCL60.JIPeZJSzsu/xHDVcx",
            "2", null),
        new SRPParams("2iQzj1CagQc/5ctbuJYLWlhtAsPHc7xWVyCPAKFRLWKADpASkqe9djWPFWTNTdeJtL8nAhImCn3Sr/IAdQ1FrGw0WvQUstPx3FO9KNcXOwisOQ1VlL.gheAHYfbYyBaxXL.NcJx9TUwgWDT0hRzFzqSrdGGTN3FgSTA1v4QnHtEygNj3eZ.u0MThqWUaDiP87nqha7XnT66bkTCkQ8.7T8L4KZjIImrNrUftedTTBi.WCi.zlrBxDuOM0da0JbUkQlXqvp0yvJAPpC11nxmmZOAbQOywZGmu9nhZNuwTlxjfIro0FOdthaDTuZRL9VL7MRPUDo/DQEyW.d4H.UIlzp",
            "2", null),
    };

    public int getPredefinedCount()
    {
        return pre_params.length;
    }
    public static SRPParams getPredefinedParams(int n)
    {
        return pre_params[n];
    }
    public static SRPParams getDefaultParams()
    {
        return pre_params[6];
    }
}
