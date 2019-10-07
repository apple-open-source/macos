//
//  SecExperimentInternal.h
//  Security
//

#ifndef SecExperimentInternal_h
#define SecExperimentInternal_h

#include <Security/SecExperimentPriv.h>

/*!
 * @function sec_experiment_run_internal
 *
 * @abstract
 *      Asynchronously run an experiment, optionally disabling sampling if desired.
 *
 *      Note: This function MUST NOT be called outside of tests.
 *
 * @param experiment_name
 *      Name of the experiment to run.
 *
 * @param sampling_disabled
 *      Flag to disable sampling.
 *
 * @param queue
 *      Queue on which to run the experiment.
 *
 * @param run_block
 *      A `sec_experiment_run_block_t` block upon which to execute the given experiment.
 */
bool
sec_experiment_run_internal(const char *experiment_name, bool sampling_disabled, dispatch_queue_t queue, sec_experiment_run_block_t run_block);

#endif /* SecExperimentInternal_h */
