/*	This header declares MeasureNetTimeInCPUCycles, which measures the
	execution time of a routine.  Comments below describe the driver routine
	that must be passed to MeasureNetTimeInCPUCyles and the other parameters
	to MeasureNetTimeInCPUCycles.

	Written by Eric Postpischil, Apple, Inc.
*/


#if !defined(apple_com_Accelerate_ClockServices_h)
#define apple_com_Accelerate_ClockServices_h


#if defined(__cplusplus)
extern "C" {
#endif


/*	Define a type describing a routine whose performance will be measured.

	The routine must take a parameter which is the number of iterations to
	execute and another parameter which is a pointer to any type of data.  To
	time any routine, write a small driver routine.  For example:

		struct MyParameters
		{
			int Argument0;
			float *Argument1;
			double Argument2;
		};


		void MyDriver(unsigned int iterations, void *data)
		{
			struct MyParameters *p = (struct MyParameters *) data;
			while (--iterations)
				MyRoutine(p->Argument0, p->Argument1, p->Argument2);
		}

	Then use MyDriver as the routine to be measured.
*/
typedef void (*RoutineToBeTimedType)(unsigned int iterations, void *data);


/*	MeasureNetTimeInCPUCycles.

	This routine measures the net amount of time it takes to execute an
	arbitrary routine.  The net time excludes overhead such as reading the
	clock value and loading the routine into cache.

	The time for multiple iterations (and multiple samples, see below) is
	measured.  This time is divided to return the time for a single iteration.

	Input:

		RoutineToBeTimedType routine.
			Address of a routine to measure.  (See typedef for this type,
			above.)

		unsigned int iterations.
			Number of iterations to be measured.

		void *data.
			Pointer to be passed to the routine.

		unsigned int samples.
			Number of samples to take.  The time a routine takes to perform
			varies due to many factors, so multiple samples are taken in an
			attempt to find the "true" time taken by the routine itself.

			On Mac OS X, the largest variance is often due to interrupts or
			process context switching.  There is no good way to exclude these
			or filter them out.  When interrupts occur, a routine takes longer
			than is required for the routine alone.  Thus, the minimum time
			observed in the samples may be the time the routine takes if no
			interrupts occur.

			By itself, this could produce an incorrectly low time.  For example,
			we might happen to start measuring just after a clock tick and
			stop just before a clock tick.  However, some fine-grain clocks are
			available, so the size of a clock tick should not bias the
			measurement too much.  Also, the measuring routine makes a control
			measurement the same way.  When N iterations are requested,
			MeasureNetTime measures the subject routine for 2 iterations and
			for N+2 iterations.  Factors that may make one measurement too low
			may also make the other measurement too low.  The measuring
			routine returns the difference, so errors should cancel to some
			extent.

	Output:
		Return value.
			Number of CPU cycles to execute one iteration.
*/
double MeasureNetTimeInCPUCycles(
		RoutineToBeTimedType routine,
		unsigned int iterations,
		void *data,
		unsigned int samples
	);


#if defined(__cplusplus)
}	// extern "C"
#endif


#endif // !defined(apple_com_Accelerate_ClockServices_h)
