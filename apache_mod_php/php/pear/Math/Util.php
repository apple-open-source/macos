<?php
//
// +----------------------------------------------------------------------+
// | PHP version 4.0                                                      |
// +----------------------------------------------------------------------+
// | Copyright (c) 1997-2001 The PHP Group                                |
// +----------------------------------------------------------------------+
// | This source file is subject to version 2.02 of the PHP license,      |
// | that is bundled with this package in the file LICENSE, and is        |
// | available at through the world-wide-web at                           |
// | http://www.php.net/license/2_02.txt.                                 |
// | If you did not receive a copy of the PHP license and are unable to   |
// | obtain it through the world-wide-web, please send a note to          |
// | license@php.net so we can mail you a copy immediately.               |
// +----------------------------------------------------------------------+
// | Authors: Sebastian Bergmann <sb@sebastian-bergmann.de>               |
// +----------------------------------------------------------------------+
//
// $Id: Util.php,v 1.1.1.3 2001/12/14 22:14:59 zarzycki Exp $
//

/**
 * Mathematic utilities
 *
 * @author  Sebastian Bergmann <sb@sebastian-bergmann.de>
 * @version $Revision: 1.1.1.3 $
 * @package Numbers
 */
class Math_Util
{
  
    /**
     * Calculates the Greatest Common Divisor (gcd) of two numbers.
     *
     * @param  int a first number
     * @param  int b second number
     * @return int gcd(a,b)
     * @access public
     */
    function gcd($a, $b)
    {
        if ($b > $a) {
            list($a, $b) = array($b, $a);
        }

        $c = 1;

        // the magic loop (thanks, Euclid :-)
        while ($c > 0) {
            $c = $a % $b;
            $a = $b;
            $b = $c;
        }

        return $a;
    }
}
?>
