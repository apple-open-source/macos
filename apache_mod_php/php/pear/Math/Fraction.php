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
// $Id: Fraction.php,v 1.1.1.2 2001/07/19 00:20:50 zarzycki Exp $
//

  /**
  * Math::Math_Fraction
  * 
  * Purpose:
  * 
  *   Class for handling fractions.
  * 
  * Example:
  * 
  *   $a = new Math_Fraction( 1, 2 );
  *   $b = new Math_Fraction( 3, 4 );
  * 
  *   $a->add( $b );
  * 
  * @author   Sebastian Bergmann <sb@sebastian-bergmann.de>
  * @version  $Revision: 1.1.1.2 $
  * @access   public
  */

  require_once "Math/Util.php";

  class Math_Fraction
  {
    // {{{ properties

    /**
    * zaehler of the fraction
    *
    * @var    int zaehler
    * @access public
    */

    var $zaehler;

    /**
    * nenner of the fraction
    *
    * @var    int nenner
    * @access public
    */

    var $nenner;

    // }}}
    // {{{ Math_Fraction( $zaehler, $nenner )

    /**
    * Constructor.
    *
    * @param  int zaehler
    * @param  int nenner
    * @access public
    */

    function Math_Fraction( $zaehler, $nenner = 1 )
    {
      $this->zaehler  = $zaehler;
      $this->nenner   = $nenner;
    }

    // }}}
    // {{{ add( $fraction, $overwrite_with_result )

    /**
    * Add another fraction to this one.
    *
    * @param  Math_Fraction fraction              
    * @param  boolean       overwrite_with_result 
    * @return Math_Fraction result (if overwrite_with_result == false)
    * @access public
    * @see    sub, mul, div
    */

    function add( $fraction, $overwrite_with_result = true )
    {
      // check, if argument is a fraction
      $fraction = $this->_check_fraction( $fraction );

      // add
      $result = new Math_Fraction( ( $this->zaehler + $fraction->zaehler ), 
                                   ( $this->nenner  * $fraction->nenner  )
                                 );

      // handle result
      return $this->_return( $result, $overwrite_with_result );
    }

    // }}}
    // {{{ sub( $fraction, $overwrite_with_result )

    /**
    * Subtract another fraction from this one.
    *
    * @param  Math_Fraction fraction              
    * @param  boolean       overwrite_with_result 
    * @return Math_Fraction result (if overwrite_with_result == false)
    * @access public
    * @see    add, mul, div
    */

    function sub( $fraction, $overwrite_with_result = true )
    {
      // check, if argument is a fraction
      $fraction = $this->_check_fraction( $fraction );

      // subtract
      $result = new Math_Fraction( ( $this->zaehler - $fraction->zaehler ), 
                                   ( $this->nenner  * $fraction->nenner  )
                                 );

      // handle result
      return $this->_return( $result, $overwrite_with_result );
    }

    // }}}
    // {{{ mul( $fraction, $overwrite_with_result )

    /**
    * Multiply another fraction with this one.
    *
    * @param  Math_Fraction fraction              
    * @param  boolean       overwrite_with_result 
    * @return Math_Fraction result (if overwrite_with_result == false)
    * @access public
    * @see    add, sub, div
    */

    function mul( $fraction, $overwrite_with_result = true )
    {
      // check, if argument is a fraction
      $fraction = $this->_check_fraction( $fraction );

      // multiply
      $result = new Math_Fraction( ( $this->zaehler * $fraction->zaehler ), 
                                   ( $this->nenner  * $fraction->nenner  )
                                 );

      // handle result
      return $this->_return( $result, $overwrite_with_result );
    }

    // }}}
    // {{{ div( $fraction, $overwrite_with_result )

    /**
    * Divide this fraction by another one.
    *
    * @param  Math_Fraction fraction              
    * @param  boolean       overwrite_with_result 
    * @return Math_Fraction result (if overwrite_with_result == false)
    * @access public
    * @see    add, sub, mul
    */

    function div( $fraction, $overwrite_with_result = true )
    {
      // check, if argument is a fraction
      $fraction = $this->_check_fraction( $fraction );

      // divide
      $result = new Math_Fraction( ( $this->zaehler * $fraction->nenner  ), 
                                   ( $this->nenner  * $fraction->zaehler )
                                 );

      // handle result
      return $this->_return( $result, $overwrite_with_result );
    }

    // }}}
    // {{{ normalize()

    /**
    * Normalize this fraction.
    *
    * @access public
    */

    function normalize()
    {
      // get greatest common divisor
      $gcd = gcd( $this->zaehler, $this->nenner );

      // divide zaehler / nenner by gcd
      $this->zaehler = $this->zaehler / $gcd;
      $this->nenner  = $this->nenner  / $gcd;
    }

    // }}}
    // {{{ dump()

    /**
    * Dump this fraction.
    *
    * @access public
    */

    function dump()
    {
      print $this->get();
    }

    // }}}
    // {{{ get()

    /**
    * Get string representation of this fraction.
    *
    * @return string  representation of fraction
    * @access public
    */

    function get()
    {
      return $this->zaehler . " / " . $this->nenner;
    }

    // }}}
    // {{{ _check_fraction( $fraction )

    /**
    * Check, if a varaible holds a Math_Fraction object.
    *
    * @param  Math_Fraction fraction to be checked
    * @return Math_Fraction checked fraction
    * @access public
    */

    function _check_fraction( $fraction )
    {
      if( get_class( $fraction ) != "math_fraction" )
      {
        $fraction = new Math_Fraction( $fraction );
      }

      return $fraction;
    }

    // }}}
    // {{{ _return( $result, $overwrite_with_result )

    /**
    * Handle the return or storage of a result from add, sub, mul or div.
    *
    * @param Math_Fraction  result
    * @param boolean        overwrite_with_result
    * @return Math_Fraction result (if overwrite_with_result == false)
    * @access public
    */

    function _return( $result, $overwrite_with_result )
    {
      $result->normalize();

      if( $overwrite_with_result )
      {
        $this->zaehler  = $result->zaehler;
        $this->nenner   = $result->nenner;
      }

      else
      {
        return $result;
      }
    }

    // }}}
  }
?>
