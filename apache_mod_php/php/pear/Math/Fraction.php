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
// $Id: Fraction.php,v 1.1.1.3 2001/12/14 22:14:58 zarzycki Exp $
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
* @version  $Revision: 1.1.1.3 $
* @access   public
* @package  Numbers
*/

require_once 'Math/Util.php';

class Math_Fraction
{
    /**
    * numerator of the fraction
    *
    * @var    int numerator
    * @access public
    */

    var $numerator;

    /**
    * denominator of the fraction
    *
    * @var    int denominator
    * @access public
    */

    var $denominator;

    /**
    * Constructor
    *
    * @param  int numerator
    * @param  int denominator
    * @access public
    */

    function Math_Fraction($numerator, $denominator = 1)
    {
        $this->numerator   = $numerator;
        $this->denominator = $denominator;
    }

    /**
    * Add another fraction to this one.
    *
    * @param  Math_Fraction fraction              
    * @param  boolean       overwrite_with_result 
    * @return Math_Fraction result (if overwrite_with_result == false)
    * @access public
    * @see    sub, mul, div
    */

    function add($fraction, $overwrite_with_result = true)
    {
        $fraction = $this->_check_fraction($fraction);

        $result = new Math_Fraction(($this->numerator   + $fraction->numerator), 
                                    ($this->denominator * $fraction->denominator)
                                   );

        return $this->_return($result, $overwrite_with_result);
    }

    /**
    * Subtract another fraction from this one.
    *
    * @param  Math_Fraction fraction              
    * @param  boolean       overwrite_with_result 
    * @return Math_Fraction result (if overwrite_with_result == false)
    * @access public
    * @see    add, mul, div
    */

    function sub($fraction, $overwrite_with_result = true)
    {
        $fraction = $this->_check_fraction($fraction);

        $result = new Math_Fraction(($this->numerator   - $fraction->numerator), 
                                    ($this->denominator * $fraction->denominator)
                                   );

        return $this->_return($result, $overwrite_with_result);
    }

    /**
    * Multiply another fraction with this one.
    *
    * @param  Math_Fraction fraction              
    * @param  boolean       overwrite_with_result 
    * @return Math_Fraction result (if overwrite_with_result == false)
    * @access public
    * @see    add, sub, div
    */

    function mul($fraction, $overwrite_with_result = true)
    {
        $fraction = $this->_check_fraction( $fraction );

        $result = new Math_Fraction(($this->numerator   * $fraction->numerator), 
                                    ($this->denominator * $fraction->denominator)
                                   );

        return $this->_return($result, $overwrite_with_result);
    }

    /**
    * Divide this fraction by another one.
    *
    * @param  Math_Fraction fraction              
    * @param  boolean       overwrite_with_result 
    * @return Math_Fraction result (if overwrite_with_result == false)
    * @access public
    * @see    add, sub, mul
    */

    function div($fraction, $overwrite_with_result = true)
    {
        $fraction = $this->_check_fraction( $fraction );

        $result = new Math_Fraction(($this->numerator   * $fraction->denominator), 
                                    ($this->denominator * $fraction->numerator)
                                   );

        return $this->_return($result, $overwrite_with_result);
    }

    /**
    * Normalize this fraction.
    *
    * @access public
    */

    function normalize()
    {
        $gcd = Math_Util::gcd($this->numerator, $this->denominator);

        $this->numerator   = $this->numerator   / $gcd;
        $this->denominator = $this->denominator / $gcd;
    }

    /**
    * Dump this fraction.
    *
    * @access public
    */

    function dump()
    {
        echo $this->get();
    }

    /**
    * Get string representation of this fraction.
    *
    * @return string  representation of fraction
    * @access public
    */

    function get()
    {
        return $this->numerator . ' / ' . $this->denominator;
    }

    /**
    * Check, if a varaible holds a Math_Fraction object.
    *
    * @param  Math_Fraction fraction to be checked
    * @return Math_Fraction checked fraction
    * @access private
    */

    function _check_fraction($fraction)
    {
        if (get_class($fraction) != 'math_fraction') {
          $fraction = new Math_Fraction( $fraction );
        }

        return $fraction;
    }

    /**
    * Handle the return or storage of a result from add, sub, mul or div.
    *
    * @param  Math_Fraction  result
    * @param  boolean        overwrite_with_result
    * @return Math_Fraction result (if overwrite_with_result == false)
    * @access private
    */

    function _return($result, $overwrite_with_result)
    {
        $result->normalize();

        if ($overwrite_with_result) {
          $this->numerator   = $result->numerator;
          $this->denominator = $result->denominator;
        } else {
          return $result;
        }
    }
}
?>
