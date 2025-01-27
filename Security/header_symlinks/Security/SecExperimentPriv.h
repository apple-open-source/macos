//
//  SecExperimentPriv.h
//  Security
//

#ifndef SecExperiment_h
#define SecExperiment_h

#include <Security/SecProtocolObject.h>
#include <xpc/xpc.h>

#ifndef SEC_EXP_OBJECT_IMPL
SEC_OBJECT_DECL(sec_experiment);
#endif // !SEC_EXP_OBJECT_IMPL

SEC_ASSUME_NONNULL_BEGIN

extern const char *kSecExperimentTLSProbe;
extern const char *kSecExperimentDefaultsDomain;

/*!
 * @block sec_experiment_run_block_t
 *
 * @abstract A block to execute an experiment with a loggable identifier
 *     and configuration. `identifier` will be uniquely associated with
 *     `experiment_config` and should be used when measuring data.
 *
 * @param identifier
 *     Identifier for the experiment.
 *
 * @param experiment_config
 *     Configuration of this experiment.
 *
 * @return True if the experiment ran successfully, and false otherwise.
 */
typedef bool (^sec_experiment_run_block_t)(const char *identifier, xpc_object_t experiment_config);

/*!
 * @block sec_experiment_skip_block_t
 *
 * @abstract A block to execute when an experiment run is skipped.
 *
 * @param identifier
 *     Identifier for the experiment.
 */
typedef void (^sec_experiment_skip_block_t)(const char *identifier);

/*!
 * @function sec_experiment_run
 *
 * @abstract
 *      Asynchronously run an experiment.
 *
 * @param experiment_name
 *      Name of the experiment to run.
 *
 * @param run_block
 *      A `sec_experiment_run_block_t` block upon which to execute the given experiment.
 *
 * @param skip_block
 *      An optional `sec_experiment_skip_block_t` block that is invoked when the chosen experiment is skipped.
 *
 * @return True if the experiment was started, and false otherwise.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
bool
sec_experiment_run(const char *experiment_name,
                   sec_experiment_run_block_t run_block,
                   sec_experiment_skip_block_t _Nullable skip_block);

#define SEC_EXPERIMENT_HAS_ASYNC_WITH_SAMPLING_DISABLED 1

/*!
 * @function sec_experiment_run_with_sampling_disabled
 *
 * @abstract
 *      Synchronously run an experiment and optionally disable sampling. Passing `true` to the `sampling_disabled` parameter
 *      will cause the experiment to always run. Clients typically SHOULD NOT do this unless running as part of test tools and utilities.
 *
 * @param experiment_name
 *      Name of the experiment to run.
 *
 * @param run_block
 *      A `sec_experiment_run_block_t` block upon which to execute the given experiment.
 *
 * @param skip_block
 *      An optional `sec_experiment_skip_block_t` block that is invoked when the chosen experiment is skipped.
 *
 * @param sampling_disabled
 *      A boolean indicating if sampling should be disabled for the given asynchronous experiment run.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
bool
sec_experiment_run_with_sampling_disabled(const char *experiment_name,
                                          sec_experiment_run_block_t run_block,
                                          sec_experiment_skip_block_t _Nullable skip_block,
                                          bool sampling_disabled);

/*!
 * @function sec_experiment_run_async
 *
 * @abstract
 *      Asynchronously run an experiment.
 *
 * @param experiment_name
 *      Name of the experiment to run.
 *
 * @param queue
 *      Queue on which to run the experiment.
 *
 * @param run_block
 *      A `sec_experiment_run_block_t` block upon which to execute the given experiment.
 *
 * @param skip_block
 *      An optional `sec_experiment_skip_block_t` block that is invoked when the chosen experiment is skipped.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
bool
sec_experiment_run_async(const char *experiment_name,
                         dispatch_queue_t queue,
                         sec_experiment_run_block_t run_block,
                         sec_experiment_skip_block_t _Nullable skip_block);

/*!
 * @function sec_experiment_run_async
 *
 * @abstract
 *      Asynchronously run an experiment and optionally disable sampling. Passing `true` to the `sampling_disabled` parameter
 *      will cause the experiment to always run. Clients typically SHOULD NOT do this unless running as part of test tools and utilities.
 *
 * @param experiment_name
 *      Name of the experiment to run.
 *
 * @param queue
 *      Queue on which to run the experiment.
 *
 * @param run_block
 *      A `sec_experiment_run_block_t` block upon which to execute the given experiment.
 *
 * @param skip_block
 *      An optional `sec_experiment_skip_block_t` block that is invoked when the chosen experiment is skipped.
 *
 * @param sampling_disabled
 *      A boolean indicating if sampling should be disabled for the given asynchronous experiment run.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
bool
sec_experiment_run_async_with_sampling_disabled(const char *experiment_name,
                                                dispatch_queue_t queue,
                                                sec_experiment_run_block_t run_block,
                                                sec_experiment_skip_block_t _Nullable skip_block,
                                                bool sampling_disabled);

/*!
 * @function sec_experiment_create
 *
 * @abstract
 *      Create an ARC-able `sec_experiment_t` instance
 *
 * @param experiment_name
 *      Name of the experiment.
 *
 * @return a `sec_experiment_t` instance.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
SEC_RETURNS_RETAINED _Nullable sec_experiment_t
sec_experiment_create(const char *experiment_name);

/*!
 * @function sec_experiment_set_sampling_disabled
 *
 * @abstract
 *      Set a flag to disable experiment sampling.
 *      This function should only be used for testing purposes.
 *
 * @param experiment
 *      A `sec_experiment_t` instance.
 *
 * @param sampling_disabled
 *      A flag indicating if sampling should be disabled.
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
void
sec_experiment_set_sampling_disabled(sec_experiment_t experiment,
                                     bool sampling_disabled);

/*!
 * @function sec_experiment_copy_configuration
 *
 * @abstract
 *      Returns the configuration dictionary associated with the given experiment.
 *
 * @param experiment
 *      A valid `sec_experiment_t` instance.
 *
 * @return  xpc_object_t containing asset bundle, if client is not part of the experiment return NULL
 */
API_AVAILABLE(macos(10.15), ios(13.0), watchos(6.0), tvos(13.0))
SEC_RETURNS_RETAINED _Nullable xpc_object_t
sec_experiment_copy_configuration(sec_experiment_t experiment);

SEC_ASSUME_NONNULL_END

#endif // SecExperiment_h
