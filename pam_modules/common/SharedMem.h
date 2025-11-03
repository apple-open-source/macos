#ifndef PAM_SHARED_MEMORY_H
#define PAM_SHARED_MEMORY_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Write data into the shared memory.
 *
 * @param data Buffer with data to write
 * @param size Number of bytes to write
 * @return 0 on success, -1 on error
 */
const char *pam_shm_write(const void* data, size_t size);

/**
 * Read data from the shared memory.
 *
 * @param handle Handle from pam_shm_write
 * @param buffer Buffer to receive data (can be NULL to query data length)
 * @param size Number of bytes to read
 * @return Number of bytes read, or -1 on failure
 */
ssize_t pam_shm_read(const char *handle, void* buffer, size_t size);

/**
 * Close the shared memory and release resources.
 *
 * @param handle Handle from pam_shm_open
 */
void pam_shm_close(const char *handle);

/**
 * Returns shared secret based on the environment name and releases shm
 *
 * @param pamh Handle to the PAM session
 * @param env_name Handle from pam_shm_open
 * @return secret value, or NULL on failure. Caller is responsible for releasing the memory
 */
char *pam_copy_secret_from_shm(struct pam_handle *pamh, const char *env_name);

/**
 * Returns shared secret with backward compatibility support
 * First tries to get the secret directly from the original PAM environment variable,
 * then falls back to shared memory approach using the new environment variable name
 *
 * @param pamh Handle to the PAM session
 * @param original_env_name Original PAM environment variable name (for backward compatibility)
 * @param shm_env_name New PAM environment variable name containing shared memory handle
 * @return secret value, or NULL on failure. Caller is responsible for releasing the memory
 */
char *pam_copy_secret_with_fallback(struct pam_handle *pamh, const char *original_env_name, const char *shm_env_name);

void pam_safe_string_release(char *str);

#ifdef __cplusplus
}
#endif

#endif // PAM_SHARED_MEMORY_H
