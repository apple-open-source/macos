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
// $Id: Iterate.php,v 1.1.1.2 2001/07/19 00:20:43 zarzycki Exp $
//

require_once 'Benchmark/Timer.php';

/**
* Benchmark::Benchmark_Iterate
* 
* Purpose:
* 
*     Benchmarking
* 
* Example:
* 
*     require_once "Benchmark/Iterate.php";
*     $benchmark = new Benchmark_Iterate;
* 
*     function foo($string)
*     {
*         print $string."<br>";
*     }
* 
*     $benchmark->run(100, 'foo', 'test');
*     $result = $benchmark->get();
* 
* @author   Sebastian Bergmann <sb@sebastian-bergmann.de>
* @version  $Revision: 1.1.1.2 $
* @access   public
*/

class Benchmark_Iterate extends Benchmark_Timer
{
    // {{{ run()

    /**
    * Benchmarks a function.
    *
    * @access public
    */

    function run()
    {
        // get arguments
        $arguments = func_get_args();
        $iterations = array_shift($arguments);
        $function_name = array_shift($arguments);

        // main loop
        for ($i = 1; $i <= $iterations; $i++)
        {
            // set 'start' marker for current iteration
            $this->setMarker('start_'.$i);

            // call function to be benchmarked
            call_user_func_array($function_name, $arguments);

            // set 'end' marker for current iteration
            $this->setMarker('end_'.$i);
        }
    }

    // }}}
    // {{{ get()

    /**
    * Returns benchmark result.
    *
    * $result[x           ] = execution time of iteration x
    * $result['mean'      ] = mean execution time
    * $result['iterations'] = number of iterations
    *
    * @return array $result
    * @access public
    */

    function get()
    {
        // init result array
        $result = array();

        // init variable
        $total = 0;

        $iterations = count($this->markers)/2;

        // loop through iterations
        for ($i = 1; $i <= $iterations; $i++)
        {
            // get elapsed time for current iteration
            $time = $this->timeElapsed('start_'.$i , 'end_'.$i);

            // sum up total time spent
            if (extension_loaded('bcmath')) {
                $total = bcadd($total, $time, 6);
            } else {
                $total = $total + $time;
            }
            
            // store time
            $result[$i] = $time;
        }

        // calculate and store mean time
        if (extension_loaded('bcmath')) {
            $result['mean'] = bcdiv($total, $iterations, 6);
        } else {
            $result['mean'] = $total / $iterations;
        }

        // store iterations
        $result['iterations'] = $iterations;

        // return result array
        return $result;
    }

    // }}}
}
?>
