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
// | Authors: Sterling Hughes <sterling@php.net>                          |
// +----------------------------------------------------------------------+
//

require_once("PEAR.php");

// {{{ Numbers_Roman

/**
 * The Numbers_Roman class provides utilities to convert roman numerals to 
 * arabic numbers and convert arabic numbers to roman numerals
 *
 * @access public
 * @author Sterling Hughes <sterling@php.net>
 * @since  PHP 4.0.5
 */
class Numbers_Roman extends PEAR
{
	// {{{ toNumber()
	
	/**
	 * Converts a roman numeral to a number
	 *
	 * @param  string  $roman The roman numeral to convert
	 *
	 * @return integer $num   The number corresponding to the
	 *                        given roman numeral
	 *
	 * @access public
	 * @author Sterling Hughes <sterling@php.net>
	 * @since  PHP 4.0.5
	 */
	function toNumber($roman)
	{
		$conv = array(
			array("letter" => 'I', "number" => 1),
			array("letter" => 'V', "number" => 5),
			array("letter" => 'X', "number" => 10),
			array("letter" => 'L', "number" => 50),
			array("letter" => 'C', "number" => 100),
			array("letter" => 'D', "number" => 500),
			array("letter" => 'M', "number" => 1000),
			array("letter" => 0,   "number" => 0)
		);
		$arabic = 0;
		$state  = 0;
		$sidx   = 0;
		$len    = strlen($roman);
	
		while ($len >= 0) {
			$i = 0;
			$sidx = $len;
			
			while ($conv[$i]['number'] > 0) {
				if (strtoupper($roman[$sidx]) == $conv[$i]['letter']) {
					if ($state > $conv[$i]['number']) {
						$arabic -= $conv[$i]['number'];
					} else {
						$arabic += $conv[$i]['number'];
						$state   = $conv[$i]['number'];
					}
				}
				$i++;
			}

			$len--;
		}
	
		return($arabic);
	}

	// }}}
	// {{{ toRoman()
	
	/**
	 * Converts a number to its roman numeral representation
	 *
	 * @param  integer $num   An integer between 0 and 3999 inclusive
	 *                        that should be converted to a roman numeral
	 *
	 * @return string  $roman The corresponding roman numeral
	 *
	 * @access public
	 * @author Sterling Hughes <sterling@php.net>
	 * @since  PHP 4.0.5
	 */
	function toRoman($num) {
		$conv = array(10 => array('X', 'C', 'M'),
		              5  => array('V', 'L', 'D'),
		              1  => array('I', 'X', 'C'));
		$roman = '';
		
		$num = (int) $num;

		$digit  = (int) $num / 1000;
		$num   -= $digit * 1000;
		while ($digit > 0) {
			$roman .= 'M';
			$digit--;
		}

		for ($i = 2; $i >= 0; $i--) {
			$power = pow(10, $i);
			$digit = (int) $num / $power;
		    $num -= $digit * $power;

			if (($digit == 9) || ($digit == 4)) {
				$roman .= $conv[1][$i] . $conv[$digit+1][$i];
			} else {
				if ($digit >= 5) {
					$roman .= $conv[5][$i];
					$digit -= 5;
				}

				while ($digit > 0) {
					$roman .= $conv[1][$i];
					$digit--;
				}
		}

		if ($num > 0) {
			return('');
		}

		return($roman);
	}
	
	// }}}
}

// }}}
?>