//
//  SecExperimentInternal.h
//  Security
//

#ifndef SecExperimentInternal_h
#define SecExperimentInternal_h

#include <Security/SecExperimentPriv.h>

SEC_ASSUME_NONNULL_BEGIN

extern const NSString *SecExperimentConfigurationKeyFleetSampleRate;
extern const NSString *SecExperimentConfigurationKeyDeviceSampleRate;
extern const NSString *SecExperimentConfigurationKeyExperimentIdentifier;
extern const NSString *SecExperimentConfigurationKeyConfigurationData;

@interface SecExperimentConfig : NSObject
@property (readonly) NSString *identifier;
@property (readonly) uint32_t fleetSampleRate;
@property (readonly) uint32_t deviceSampleRate;
@property (readonly) NSDictionary *configurationData;
@property (readonly) BOOL isSampled;
- (instancetype)initWithConfiguration:(NSDictionary *)configuration;

// Note: these functions are exposed for testing purposes only.
- (uint32_t)hostHash;
- (BOOL)shouldRunWithSamplingRate:(NSNumber *)sampleRate;
@end

@interface SecExperiment : NSObject
@property (readonly) NSString *name;
@property (readonly, nullable) NSString *identifier;
@property (readonly) BOOL samplingDisabled;
- (instancetype)initWithName:(const char *)name;
- (BOOL)experimentIsAllowedForProcess;
- (BOOL)isSamplingDisabledWithDefault:(BOOL)defaultValue;
- (BOOL)isSamplingDisabled;
- (SecExperimentConfig *)copyExperimentConfiguration;

// Note: These functions are exposed for testing purposes only.
- (NSDictionary *)copyExperimentConfigurationFromUserDefaults;
- (NSDictionary *)copyRemoteExperimentAsset;
- (NSDictionary *)copyRandomExperimentConfigurationFromAsset:(NSDictionary *)asset;
@end

/*!
 * @function sec_experiment_create_with_inner_experiment
 *
 * @abstract
 *      Create an ARC-able `sec_experiment_t` instance wrapping an internal `SecExperiment` object.
 *
 * @param experiment
 *      The experiment
 *
 * @return a `sec_experiment_t` instance.
 */
SEC_RETURNS_RETAINED _Nullable sec_experiment_t
sec_experiment_create_with_inner_experiment(SecExperiment *experiment);

/*!
 * @function sec_experiment_run_internal
 *
 * @abstract
 *      Asynchronously run an experiment, optionally disabling sampling if desired.
 *
 *      Note: This function MUST NOT be called outside of tests.
 *
 * @param experiment
 *      A `sec_experiment_t` to run.
 *
 * @param sampling_disabled
 *      Flag to disable sampling.
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
 * @param synchronous
 *      A boolean indicating if the given experiment should be run synchronously (true) or asynchronously (false).
 *
 * @return True if the experiment was started, and false otherwise.
 */
bool
sec_experiment_run_internal(sec_experiment_t experiment,
                            bool sampling_disabled,
                            dispatch_queue_t _Nullable queue,
                            sec_experiment_run_block_t run_block,
                            sec_experiment_skip_block_t _Nullable skip_block,
                            bool synchronous);

/*!
* @function sec_experiment_get_run_count
*
* @abstract
*      Determine the number of times an experiment ran.
*
* @param experiment
*      A `sec_experiment_t` instance.
*
* @return Number of times the experiment was run.
*/
size_t
sec_experiment_get_run_count(sec_experiment_t experiment);

/*!
 * @function sec_experiment_get_successful_run_count
 *
 * @abstract
 *      Determine the number of times an experiment successfully ran.
 *
 * @param experiment
 *      A `sec_experiment_t` instance.
 *
 * @return Number of times the experiment was run successfully.
*/
size_t
sec_experiment_get_successful_run_count(sec_experiment_t experiment);

/*!
 * @function sec_experiment_get_identifier
 *
 * @abstract
 *      Get the selected experiment identifier.
 *
 * @param experiment
 *      A `sec_experiment_t` instance.
 *
 * @return The internal experiment identifier, or NULL if one was not yet chosen.
*/
const char * _Nullable
sec_experiment_get_identifier(sec_experiment_t experiment);

SEC_ASSUME_NONNULL_END

#endif /* SecExperimentInternal_h */
