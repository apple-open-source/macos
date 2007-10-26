/*
 * Copyright 2005-2006 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#ifndef KIM_SELECTION_HINTS_H
#define KIM_SELECTION_HINTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kim/kim_types.h>
    
/*!
 * \page kim_selection_hints_overview KIM Selection Hints Overview
 *
 * \section kim_selection_hints_introduction Introduction
 *
 * Most users belong to multiple organizations and thus need
 * to authenticate to multiple Kerberos realms.  Traditionally Kerberos sites 
 * solved this problem by setting up a cross-realm relationship, which allowed 
 * the user to use TGT credentials for their client identity in one realm 
 * to obtain credentials in another realm via cross-realm authentication.  As a 
 * result users could acquire credentials for a single client identity and use 
 * them everywhere.
 *
 * Setting up cross-realm requires that realms share a secret, so sites must 
 * coordinate with one another to set up a cross-realm relationship.  In 
 * addition, sites must set up authorization policies for users from other  
 * realms.  As Kerberos becomes increasingly wide-spread, many realms will 
 * not have cross-realm relationships, and users will need to   
 * manually obtain credentials for their client identity at each realm
 * (eg: "user@BANK.COM", "user@UNIVERSITY.EDU", etc).  As a result, users 
 * will often have multiple credentials caches, one for each client identity.
 *
 * Unfortunately this presents a problem for applications which need to obtain
 * service credentials.  Which client identity should they use?  
 * Rather than having each application to manually search the cache collection,
 * KIM provides a selection hints API for choosing the best client identity.  
 * This API is intended to simplify the process of choosing credentials 
 * and provide consistent behavior across all applications.
 *
 * Searching the cache collection for credentials may be expensive if there
 * are a large number of caches.  If credentials for the client identity 
 * are expired or not present, KIM may also wish to prompt the user for
 * new credentials for the appropriate client identity.  As a result, 
 * applications might want to remember which client identity worked in
 * the past and always request credentials using that identity.  
 * 
 *
 * \section kim_selection_hints_creating Creating KIM Selection Hints
 * 
 * A KIM selection hints object consists of an application identifier and one or 
 * more pieces of information about the service the client application will be 
 * contacting.  The application identifier is used by user preferences 
 * to control how applications share cache entries.  It is important to be
 * consistent about what application identifier you provide.  Java-style  
 * identifiers are recommended to avoid collisions.
 *
 * \section kim_selection_hints_searching Selection Hint Search Behavior
 *
 * When using selection hints to search for an appropriate client identity, 
 * KIM uses a consistent hint search order.  This allows applications to specify 
 * potentially contradictory information without preventing KIM from locating a 
 * single ccache.  In addition the selection hint search order may change, 
 * especially if more hints are added.  
 *
 * As a result, callers are encouraged to provide all relevant search hints, 
 * even if only a subset of those search hints are necessary to get reasonable 
 * behavior in the current implementation.  Doing so will provide the most
 * user-friendly selection experience.
 *
 * Currently the search order looks like this:
 *
 * \li <B>Service Identity</B> The client identity which has obtained a service credential for this service identity.
 * \li <B>Hostname</B> A client identity which has obtained a service credential for this server.
 * \li <B>Service Realm</B> A client identity which has obtained a service credential for this realm.
 * \li <B>Service</B> A client identity which has obtained a service credential for this service.
 * \li <B>Client Realm</B> A client identity in this realm.
 * \li <B>User</B> A client identity whose first component is this user string.
 *
 * For example, if you specify a service identity and a credential for 
 * that identity already exists in the ccache collection, KIM may use that 
 * ccache, even if your user and client realm entries in the selection hints would  
 * lead it to choose a different ccache.  If no credentials for the service identity
 * exist then KIM will fall back on the user and realm hints.
 *
 * \note Due to performance and information exposure concerns, currently all 
 * searching is done by examining the cache collection.  In the future the KIM 
 * may also make network requests as part of its search algorithm.  For example
 * it might check to see if the TGT credentials in each ccache can obtain
 * credentials for the service identity specified by the selection hints.
 *
 * \section kim_selection_hints_selecting Selecting an Identity Using Selection Hints
 *
 * Once you have provided search criteria for selecting an identity, use
 * #kim_selection_hints_get_identity() to obtain an identity object.  
 * You can then use #kim_identity_get_gss_name() to obtain a gss_name_t 
 * for use in gss_acquire_cred() or use 
 * #kim_ccache_create_from_client_identity() to obtain a ccache containing 
 * credentials for the identity.
 *
 * \note #kim_selection_hints_get_identity() obtains an identity based on
 * the current state of the selection hints object.  Subsequent changes
 * to the selection hints object will not change the identity.
 *
 * \section kim_selection_hints_caching Selection Hint Caching Behavior
 * 
 * In addition to using selection hints to search for an appropriate client
 * identity, KIM can also use them to remember which client identity worked.  
 * When the client identity returned by KIM successfully authenticates to  
 * the server and passes authorization checks, you may use 
 * #kim_selection_hints_cache_results() to add a mapping to the cache,
 * replacing an existing mapping if there is one.  Future calls to KIM using 
 * identical selection hints will result in the cached client identity and 
 * no searching will occur.
 * 
 * If a cache entry becomes invalid, you can forget the mapping  
 * using #kim_selection_hints_forget_cached_results().  If you don't 
 * want to remove the mapping but also don't want to use it, you can turn 
 * off querying of the cache for a particular selection hints object using 
 * #kim_selection_hints_set_use_cached_results().  
 *
 * \note Because cache entries key off of selection hints, it is important
 * to always specify the same hints when contacting a particular
 * service.  Otherwise KIM will not always find the cache entries.
 *
 * \section kim_selection_hints_prompt Selection Hint Prompting Behavior
 * 
 * If valid credentials for identity in the selection hints cache are
 * unavailable or if no identity could be found using searching or caching
 * when #kim_selection_hints_get_identity() is called, KIM may present a 
 * GUI to ask the user to select an identity or acquire credentials for 
 * an identity.  
 *
 * \note So long as the caller caches the identity with 
 * #kim_selection_hints_cache_results() the user will be prompted only
 * when setting up the application or when credentials expire.
 *
 * In order to let the user know why Kerberos needs their assistance, KIM  
 * displays the name of the application which requested the identity   
 * selection. Unfortunately, some platforms do not provide a runtime 
 * mechanism for determining the name of the calling process.  If your 
 * application runs on one of these platforms (or is cross-platform) 
 * you should provide a localized version of its name with 
 * #kim_selection_hints_set_application_name().  You can check what name 
 * will be used with #kim_selection_hints_get_application_name().
 *
 * In many cases a single application may select different identities for 
 * different purposes.  For example an email application might use different 
 * identities to check mail for different accounts.  If your application 
 * has this property you may need to provide the user with a localized 
 * string describing how the identity will be used.  You can specify 
 * this string with #kim_selection_hints_get_explanation().  You can find 
 * out what string will be used with kim_selection_hints_set_explanation().
 *
 * Since the user may choose to acquire credentials when selection an
 * identity, KIM also provides #kim_selection_hints_set_options() to 
 * set what credential acquisition options are used.  
 * #kim_selection_hints_get_options() returns the options which will be used. 
 *
 * If you need to disable user interaction, use 
 * #kim_selection_hints_set_allow_user_interaction.  Use 
 * #kim_selection_hints_get_allow_user_interaction to find out whether or
 * not user interaction is enabled.  User interaction is enabled by default.
 *
 * See \ref kim_selection_hints_reference for information on specific APIs.
 */

/*!
 * \defgroup kim_selection_hints_reference KIM Selection Hints Reference Documentation
 * @{
 */

/*!
 * \param out_selection_hints       on exit, a new selection hints object.  
 *                                  Must be freed with kim_selection_hints_free().
 * \param in_application_identifier an application identifier string.  Java-style identifiers are recommended 
 *                                  to avoid cache entry collisions (eg: "com.example.MyApplication")
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Create a new selection hints object.
 */
kim_error_t kim_selection_hints_create (kim_selection_hints_t *out_selection_hints,
                                        kim_string_t           in_application_identifier);

/*!
 * \param out_selection_hints on exit, a new selection hints object which is a copy of in_selection_hints.  
 *                            Must be freed with kim_selection_hints_free().
 * \param in_selection_hints  a selection hints object. 
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Copy a selection hints object.
 */
kim_error_t kim_selection_hints_copy (kim_selection_hints_t *out_selection_hints,
                                      kim_selection_hints_t  in_selection_hints);

/*!
 * \param io_selection_hints   a selection hints object to modify.
 * \param in_service_identity  a service identity.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set the preferred service identity.
 * \sa kim_selection_hints_get_service_identity()
 */
kim_error_t kim_selection_hints_set_service_identity (kim_selection_hints_t io_selection_hints,
                                                      kim_identity_t        in_service_identity);

/*!
 * \param in_selection_hints    a selection hints object.
 * \param out_service_identity  on exit, the service identity specified in \a in_selection_hints.
 *                              Must be freed with kim_identity_free().
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the preferred service identity.
 * \sa kim_selection_hints_set_service_identity()
 */
kim_error_t kim_selection_hints_get_service_identity (kim_selection_hints_t  in_selection_hints,
                                                      kim_identity_t        *out_service_identity);

/*!
 * \param io_selection_hints a selection hints object to modify.
 * \param in_client_realm    a client realm string.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set the preferred client realm.
 * \sa kim_selection_hints_get_client_realm()
 */
kim_error_t kim_selection_hints_set_client_realm (kim_selection_hints_t io_selection_hints,
                                                  kim_string_t          in_client_realm);

/*!
 * \param in_selection_hints a selection hints object.
 * \param out_client_realm   on exit, the client realm string specified in \a in_selection_hints.
 *                           Must be freed with kim_string_free().
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the preferred client realm.
 * \sa kim_selection_hints_set_client_realm()
 */
kim_error_t kim_selection_hints_get_client_realm (kim_selection_hints_t  in_selection_hints,
                                                  kim_string_t          *out_client_realm);

/*!
 * \param io_selection_hints a selection hints object to modify.
 * \param in_user            a user name string.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set the preferred user name.
 * \sa kim_selection_hints_get_user()
 */
kim_error_t kim_selection_hints_set_user (kim_selection_hints_t io_selection_hints,
                                          kim_string_t          in_user);

/*!
 * \param in_selection_hints a selection hints object.
 * \param out_user           on exit, the user name string specified in \a in_selection_hints.
 *                           Must be freed with kim_string_free().
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the preferred user name.
 * \sa kim_selection_hints_set_user()
 */
kim_error_t kim_selection_hints_get_user (kim_selection_hints_t  in_selection_hints,
                                          kim_string_t          *out_user);


/*!
 * \param io_selection_hints a selection hints object to modify.
 * \param in_service_realm    a service realm string.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set the preferred service realm.
 * \sa kim_selection_hints_get_service_realm()
 */
kim_error_t kim_selection_hints_set_service_realm (kim_selection_hints_t io_selection_hints,
                                                   kim_string_t          in_service_realm);

/*!
 * \param io_selection_hints a selection hints object.
 * \param out_service_realm  on exit, the service realm string specified in \a in_selection_hints.
 *                           Must be freed with kim_string_free().
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the preferred service realm.
 * \sa kim_selection_hints_set_service_realm()
 */
kim_error_t kim_selection_hints_get_service_realm (kim_selection_hints_t  io_selection_hints,
                                                   kim_string_t          *out_service_realm);

/*!
 * \param io_selection_hints a selection hints object to modify.
 * \param in_service         a service name string.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set the preferred service name.
 * \sa kim_selection_hints_get_service()
 */
kim_error_t kim_selection_hints_set_service (kim_selection_hints_t io_selection_hints,
                                             kim_string_t          in_service);

/*!
 * \param in_selection_hints a selection hints object.
 * \param out_service        on exit, the service name string specified in \a in_selection_hints.
 *                           Must be freed with kim_string_free().
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the preferred service name.
 * \sa kim_selection_hints_set_service()
 */
kim_error_t kim_selection_hints_get_service (kim_selection_hints_t  in_selection_hints,
                                             kim_string_t          *out_service);

/*!
 * \param io_selection_hints a selection hints object to modify.
 * \param in_hostname        a server host name string.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set the preferred server host name.
 * \sa kim_selection_hints_get_hostname()
 */
kim_error_t kim_selection_hints_set_hostname (kim_selection_hints_t io_selection_hints,
                                              kim_string_t          in_hostname);

/*!
 * \param in_selection_hints a selection hints object.
 * \param out_hostname       on exit, the server host name string specified in \a in_selection_hints.
 *                           Must be freed with kim_string_free().
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the preferred server host name.
 * \sa kim_selection_hints_set_hostname()
 */
kim_error_t kim_selection_hints_get_hostname (kim_selection_hints_t  in_selection_hints,
                                              kim_string_t          *out_hostname);

/*!
 * \param io_selection_hints  a selection hints object to modify.
 * \param in_application_name a localized string containing the full name of the application.
 * \note If you do not call this function KIM will attempt to determine the application
 * name at runtime.  If that fails (the functionality is only available on some platforms)
 * then KIM will use the application identity string.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set the application name for use in user interaction.
 * \sa kim_selection_hints_get_application_name()
 */
kim_error_t kim_selection_hints_set_application_name (kim_selection_hints_t io_selection_hints,
                                                      kim_string_t          in_application_name);

/*!
 * \param in_selection_hints   a selection hints object.
 * \param out_application_name on exit, the localized full name of the application specified 
 *                             in \a in_selection_hints. Must be freed with kim_string_free().
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the application name for use in user interaction.
 * \sa kim_selection_hints_set_application_name()
 */
kim_error_t kim_selection_hints_get_application_name (kim_selection_hints_t  in_selection_hints,
                                                      kim_string_t          *out_application_name);

/*!
 * \param io_selection_hints  a selection hints object to modify.
 * \param in_explanation      a localized string describing why the caller needs the identity.
 * \note If the application only does one thing (the reason it needs an identity is obvious) 
 * then you may not need to call this function.  You may still need to call 
 * #kim_selection_hints_set_application_name()
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set the strings used to prompt the user to select the identity.
 * \sa kim_selection_hints_get_explanation()
 */
kim_error_t kim_selection_hints_set_explanation (kim_selection_hints_t io_selection_hints,
                                                 kim_string_t          in_explanation);

/*!
 * \param in_selection_hints   a selection hints object.
 * \param out_explanation      on exit, the localized string specified in \a in_selection_hints
 *                             which describes why the caller needs the identity.  May be NULL.
 *                             If non-NULL, must be freed with kim_string_free().
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the strings used to prompt the user to select the identity.
 * \sa kim_selection_hints_set_explanation()
 */
kim_error_t kim_selection_hints_get_explanation (kim_selection_hints_t  in_selection_hints,
                                                 kim_string_t          *out_explanation);


/*!
 * \param io_selection_hints  a selection hints object to modify.
 * \param in_options          options to control credential acquisition. 
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set the options which will be used if credentials need to be acquired.
 * \sa kim_selection_hints_get_options()
 */
kim_error_t kim_selection_hints_set_options (kim_selection_hints_t io_selection_hints,
                                             kim_options_t         in_options);

/*!
 * \param in_selection_hints a selection hints object.
 * \param out_options        on exit, the options to control credential acquisition  
 *                           specified in \a in_selection_hints.  May be KIM_OPTIONS_DEFAULT.
 *                           If not, must be freed with kim_options_free().
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the options which will be used if credentials need to be acquired.
 * \sa kim_selection_hints_set_options()
 */
kim_error_t kim_selection_hints_get_options (kim_selection_hints_t  in_selection_hints,
                                             kim_options_t         *out_options);

/*!
 * \param in_selection_hints        a selection hints object to modify
 * \param in_allow_user_interaction a boolean value specifying whether or not KIM should ask
 *                                  the user to select an identity for \a in_selection_hints.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \note This setting defaults to TRUE.
 * \brief Set whether or not KIM may interact with the user to select an identity.
 * \sa kim_selection_hints_get_allow_user_interaction
 */
kim_error_t kim_selection_hints_set_allow_user_interaction (kim_selection_hints_t in_selection_hints,
                                                            kim_boolean_t         in_allow_user_interaction);

/*!
 * \param in_selection_hints         a selection hints object to modify
 * \param out_allow_user_interaction on exit, a boolean value specifying whether or not KIM 
 *                                   should ask the user to select an identity for 
 *                                   \a in_selection_hints.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \note This setting defaults to TRUE.
 * \brief Get whether or not KIM may interact with the user to select an identity.
 * \sa kim_selection_hints_set_allow_user_interaction
 */
kim_error_t kim_selection_hints_get_allow_user_interaction (kim_selection_hints_t  in_selection_hints,
                                                            kim_boolean_t         *out_allow_user_interaction);

/*!
 * \param in_selection_hints    a selection hints object to modify
 * \param in_use_cached_results a boolean value specifying whether or not KIM should use a cached
 *                              mapping between \a in_selection_hints and a Kerberos identity.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \note This setting defaults to TRUE.
 * \brief Set whether or not KIM will cache mappings generated by this selection hints object.
 * \sa kim_selection_hints_get_use_cached_results
 */
kim_error_t kim_selection_hints_set_use_cached_results (kim_selection_hints_t in_selection_hints,
                                                        kim_boolean_t         in_use_cached_results);

/*!
 * \param in_selection_hints     a selection hints object to modify
 * \param out_use_cached_results on exit, a boolean value specifying whether or not KIM will use a 
 *                               cached mapping between \a in_selection_hints and a Kerberos identity.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \note This setting defaults to TRUE.
 * \brief Get whether or not KIM will cache mappings generated by this selection hints object.
 * \sa kim_selection_hints_set_use_cached_results
 */
kim_error_t kim_selection_hints_get_use_cached_results (kim_selection_hints_t  in_selection_hints,
                                                        kim_boolean_t         *out_use_cached_results);

/*!
 * \param in_selection_hints the selection hints to add to the cache.
 * \param out_identity       the Kerberos identity \a in_selection_hints maps to.
 *                           Must be freed with kim_identity_free().
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \note \a out_identity is the identity mapped to by the current state of \a in_selection_hints.
 * This function may prompt the user via a GUI to choose that identity.
 * Subsequent modifications to \a in_selection_hints will not change \a out_identity.
 * \brief Choose a client identity based on selection hints.
 */

kim_error_t kim_selection_hints_get_identity (kim_selection_hints_t in_selection_hints,
                                              kim_identity_t        *out_identity);

/*!
 * \param in_selection_hints the selection hints to add to the cache.
 * \param in_identity the Kerberos identity \a in_selection_hints maps to.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Add an entry for the selection hints to the selection hints cache, 
 * replacing any existing entry.
 */

kim_error_t kim_selection_hints_cache_results (kim_selection_hints_t in_selection_hints,
                                               kim_identity_t        in_identity);

/*!
 * \param in_selection_hints the selection hints to remove from the cache.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Remove an entry for the selection hints from the selection hints cache.
 */

kim_error_t kim_selection_hints_forget_cached_results (kim_selection_hints_t in_selection_hints);

/*!
 * \param io_selection_hints the selection hints object to be freed.  Set to NULL on exit.
 * \brief Free memory associated with a selection hints object.
 */

void kim_selection_hints_free (kim_selection_hints_t *io_selection_hints);

/*!@}*/

#ifdef __cplusplus
}
#endif

#endif /* KIM_SELECTION_HINTS_H */
