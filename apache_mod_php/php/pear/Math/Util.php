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
// $Id: Util.php,v 1.1.1.2 2001/07/19 00:20:50 zarzycki Exp $
//

  // {{{ gcd( $a, $b )

  /**
  * Calculates the Greatest Common Divisor (gcd) of two numbers.
  *
  * @param  int a first number
  * @param  int b second number
  * @return int result gcd(a,b)
  * @access public
  */

  function gcd( $a, $b )
  {
    // make sure that $a > $b holds true
    if( $b > $a )
    {
      // swap $a and $b
      list( $a, $b ) = array( $b, $a );
    }

    // init $c to 1
    $c = 1;

    // the magic loop (thanks, Euclid :-)
    while( $c > 0 )
    {
      $c = $a % $b;
      $a = $b;
      $b = $c;
    }

    // return result
    return $a;
  }

  // }}}
?>
