/* The currently known Mac OS X deployment targets */
enum macosx_deployment_target_value {
    MACOSX_DEPLOYMENT_TARGET_10_1,
    MACOSX_DEPLOYMENT_TARGET_10_2,
    MACOSX_DEPLOYMENT_TARGET_10_3
};

__private_extern__ void get_macosx_deployment_target(
    enum macosx_deployment_target_value *value,
    const char **name);
