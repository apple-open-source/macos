<?php
//
// +----------------------------------------------------------------------+
// | PHP version 4.0                                                      |
// +----------------------------------------------------------------------+
// | Copyright (c) 1997, 1998, 1999, 2000 The PHP Group                   |
// +----------------------------------------------------------------------+
// | This source file is subject to version 2.02 of the PHP license,      |
// | that is bundled with this package in the file LICENSE, and is        |
// | available at through the world-wide-web at                           |
// | http://www.php.net/license/2_02.txt.                                 |
// | If you did not receive a copy of the PHP license and are unable to   |
// | obtain it through the world-wide-web, please send a note to          |
// | license@php.net so we can mail you a copy immediately.               |
// +----------------------------------------------------------------------+
// | Authors: Sebastian Bergmann <sb@phpOpenTracker.de>                   |
// +----------------------------------------------------------------------+
//
// $Id: Iterate.php,v 1.1.1.1 2001/01/25 05:00:28 wsanchez Exp $
//

  require_once "Benchmark/Timer.php";

  /**
  * Benchmark::Benchmark_Iterate
  * 
  * Purpose:
  * 
  *   Benchmarking
  * 
  * Example:
  * 
  *   $benchmark = new Benchmark_Iterate;
  * 
  *   $benchmark->run( "my_function", 100 );
  * 
  *   $result = $benchmark->get();
  * 
  * @author   Sebastian Bergmann <sb@phpOpenTracker.de>
  * @version  $Revision: 1.1.1.1 $
  * @access   public
  */

  class Benchmark_Iterate extends Benchmark_Timer
  {
    // {{{ run()

    /**
    * Benchmarks a function.
    *
    * @param  string  $function   name of the function to be benchmarked
    * @param  int     $iterations number of iterations (default: 100)
    * @access public
    */

    function run( $function, $iterations = 100 )
    {
      for( $i = 1; $i <= $iterations; $i++ )
      {
        $this->set_marker( "start_" . $i );

        call_user_func( $function );

        $this->set_marker( "end_" . $i );
      }
    }

    // }}}
    // {{{ get()

    /**
    * Returns benchmark result.
    *
    * $result[ x            ] = execution time of iteration x
    * $result[ "mean"       ] = mean execution time
    * $result[ "iterations" ] = number of iterations
    *
    * @return array $result
    * @access public
    */

    function get()
    {
      $result = array();
      $total = 0;

      $iterations = count( $this->markers ) / 2;

      for( $i = 1; $i <= $iterations; $i++ )
      {
        $time = $this->time_elapsed( "start_" . $i , "end_" . $i );

        $total = bcadd( $total, $time, 6 );
        $result[ $i ] = $time;
      }

      $result[ "mean" ] = bcdiv( $total, $iterations, 6 );
      $result[ "iterations" ] = $iterations;

      return $result;
    }

    // }}}
  }
?>
