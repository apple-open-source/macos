/*
 * Copyright (c) 2010 - 2023 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <netsmb/smb_lib.h>
#include <smbclient/smbclient.h>
#include <smbclient/smbclient_internal.h>
#include "rcfile.h"
#include "preference.h"
#include "smb_preferences.h"
#include <net/if.h>

#define defaultCodePage  437


/*
 * level values:
 * 0 - default/global
 * 1 - server
 * 2 - server:share
 */
static void readPreferenceSection(struct rcfile *rcfile, struct smb_prefs *prefs, 
								   const char *sname, int level)
{
	char *p = NULL;
	int32_t	altflags = 0;
    char *str = NULL;
    char *token = NULL;
    uint32_t if_index = 0;
    uint32_t ignorelist_len = 0;
    uint32_t list_len = 0;
    size_t str_len = 0;

	/* global only preferences */
	if (level == 0) {
		/* 
		 * Neither of these are defined in the man pages. We should add
		 * kernel log level, then in future remove debug level.
		 */
		rc_getint(rcfile, sname, "debug_level", &prefs->KernelLogLevel);
		rc_getint(rcfile, sname, "kloglevel", &prefs->KernelLogLevel);

        if (rc_getbool(rcfile, sname, "connect_to_sharedisk", &altflags) == 0) {
            if (altflags)
                prefs->altflags &= ~SMBFS_MNT_HIFI_DISABLED;
            else
                prefs->altflags |= SMBFS_MNT_HIFI_DISABLED;
        }
	}
	
	/* server only preferences */
	if (level == 1) {
		rc_getstringptr(rcfile, sname, "addr", &p);
		if (p) {
			if (prefs->NetBIOSDNSName) {
				CFRelease(prefs->NetBIOSDNSName);
			}
			prefs->NetBIOSDNSName = CFStringCreateWithCString(kCFAllocatorSystemDefault, p, kCFStringEncodingUTF8);
		}
	}
	
	/* global or server preferences */
	if ((level == 0) || (level == 1)) {
        /*
         * Check for which versions of SMB are enabled.
         * Default is only SMB 2/3 are enabled
         *
         * 1 = 0001 = SMB 1 enabled
         * 3 = 0011 = SMB 1 and 2 enabled
         * 6 = 0110 = SMB 2 and 3 enabled <default>
         * 7 = 0111 = SMB 1 and 2 and 3 enabled
         */
        /* Check for which versions of SMB we support */
        rc_getint(rcfile, sname, "protocol_vers_map", &prefs->protocol_version_map);
        if (prefs->protocol_version_map == 0) {
            /* 0 is invalid so assume the safer of SMB 2/3 only */
            prefs->protocol_version_map = 6;
        }

		rc_getint(rcfile, sname, "nbtimeout", &prefs->NetBIOSResolverTimeout);
		/* Make sure they set it to something */
		if (prefs->NetBIOSResolverTimeout == 0) {
			prefs->NetBIOSResolverTimeout = DefaultNetBIOSResolverTimeout;
		}

		/* 
		 * We default to tryBothPorts = FALSE and port SMB_TCP_PORT_445.
         *
         * Be careful because a port can be specified in the URL to use
         * which overrides this preference. SetPortNumberFromURL() will set
         * tryBothPorts = FALSE and set tcp_port to the port from the URL.
         *
         * A Bonjour name resolution will also disable netBios
         *
         * If they want to try both port 445 and port 139, then they need to
         * set the "port445=both"
		 */
		if (prefs->tcp_port == SMB_TCP_PORT_445) {
			rc_getstringptr(rcfile, sname, "port445", &p);
			/* See if the configuration file wants us to use a specific port.  */
			if (p) {
				if (strcmp(p, "netbios_only") == 0) {
					prefs->tryBothPorts = FALSE;
					prefs->tcp_port = NBSS_TCP_PORT_139;
				}
				else if (strcmp(p, "no_netbios") == 0) {
					prefs->tryBothPorts = FALSE;
					prefs->tcp_port = SMB_TCP_PORT_445;
				}
                else if (strcmp(p, "both") == 0) {
                    os_log_debug(OS_LOG_DEFAULT, "%s: Both ports on",
                                 __FUNCTION__);

                    prefs->tryBothPorts = TRUE;
                    prefs->tcp_port = SMB_TCP_PORT_445;
                }
			}
		}

		/* Really should be getting this from System Configuration */
		rc_getstringptr(rcfile, sname, "minauth", &p);
		if (p) {
			/*
			 * "minauth" was set in this section; override
			 * the current minimum authentication setting.
			 */
			if (strcmp(p, "kerberos") == 0) {
				/*
				 * Don't fall back to NTLMv2, NTLMv1, or
				 * a clear text password.
				 */
				prefs->minAuthAllowed = SMB_MINAUTH_KERBEROS;
			} else if (strcmp(p, "ntlmv2") == 0) {
				/*
				 * Don't fall back to NTLMv1 or a clear
				 * text password.
				 */
				prefs->minAuthAllowed = SMB_MINAUTH_NTLMV2;
			} else if (strcmp(p, "ntlm") == 0) {
				/*
				 * Don't send the LM response over the wire.
				 */
				prefs->minAuthAllowed = SMB_MINAUTH_NTLM;
			} else if (strcmp(p, "lm") == 0) {
				/*
				 * Fail if the server doesn't do encrypted
				 * passwords.
				 */
				prefs->minAuthAllowed = SMB_MINAUTH_LM;
			} else if (strcmp(p, "none") == 0) {
				/*
				 * Anything goes.
				 * (The following statement should be
				 * optimized away.)
				 */
				prefs->minAuthAllowed = SMB_MINAUTH;
			}
		}
        
        rc_getint(rcfile, sname, "max_resp_timeout", &prefs->max_resp_timeout);
		/* Make sure they set it to something reasonable */
		if (prefs->max_resp_timeout > 600) {
			prefs->max_resp_timeout = 600; /* 10 mins is a long, long time */
		}
        
        /* Check for Required Signing */
        /* Only get the value if it exist, ignore any error we don't care */
        (void)rc_getbool(rcfile, sname, "signing_required", (int *) &prefs->signing_required);

        /*
         * Check for which signing algorithms of SMB signing are enabled.
         * SMB2_SIGNING_AES_CMAC 0x0001
         * SMB2_SIGNING_AES_GMAC 0x0002
         */
        rc_getint(rcfile, sname, "signing_alg_map", &prefs->signing_algorithm_map);
        if (prefs->signing_algorithm_map == 0) {
            /* 0 is invalid so turn on all algorithms */
            prefs->signing_algorithm_map = 0x3;
        }

        /* Check for which versions of SMB require signing */
        rc_getint(rcfile, sname, "signing_req_vers", &prefs->signing_req_versions);

        /* Only get the value if it exists */
        if (rc_getbool(rcfile, sname, "validate_neg_off", &altflags) == 0) {
            if (altflags)
                prefs->altflags |= SMBFS_MNT_VALIDATE_NEG_OFF;
            else
                prefs->altflags &= ~SMBFS_MNT_VALIDATE_NEG_OFF;
        }
        
        /*
         * Check for which encryption algorithms of SMB encryption are enabled.
         * AES_128_CCM is always enabled (0x0001)
         * AES_128_GCM is enabled (0x0002)
         * AES_256_CCM (0x0004) is enabled
         * AES_256_GCM (0x0008) is enabled
         */
        rc_getint(rcfile, sname, "encrypt_cipher_map", &prefs->encrypt_algorithm_map);
        if (prefs->encrypt_algorithm_map == 0) {
            /* 0 is invalid so assume AES_128_CCM/AES_128_GCM/AES-256-CCM/AES-256-GCM */
            prefs->encrypt_algorithm_map = 0xf;
        }
        
        /* Check for forcing encryption */
        /* Only get the value if it exist, ignore any error we don't care */
        (void)rc_getbool(rcfile, sname, "force_sess_encrypt",
                         (int *) &prefs->force_sess_encrypt);
        (void)rc_getbool(rcfile, sname, "force_share_encrypt",
                         (int *) &prefs->force_share_encrypt);
        
        
        /* Multichannel preferences (enabled by default) */
        if (rc_getbool(rcfile, sname, "mc_on", &altflags) == 0) {
            if (!altflags) {
                prefs->altflags &= ~SMBFS_MNT_MULTI_CHANNEL_ON;
            }
        }

        /*
         * OFF by default and if set, then in all cases wired NICs will be used as
         * active channels and wireless NICs will be possibly set as an inactive in
         * case no wired NIC should be set as inactive
         */
        if (rc_getbool(rcfile, sname, "mc_prefer_wired", &altflags) == 0) {
            if (altflags) {
                prefs->altflags |= SMBFS_MNT_MC_PREFER_WIRED;
            }
        }
        
        /* Is chained compression disabled? */
        if (rc_getbool(rcfile, sname, "comp_chaining_disable", &altflags) == 0) {
            if (altflags) {
                prefs->altflags |= SMBFS_MNT_COMPRESSION_CHAINING_OFF;
            }
        }

        /*
         * Check for which compression algorithms are enabled.
         * Default is all algorithms are enabled
         *
         * SMB2_COMPRESSION_LZNT1_ENABLED (0x00000001) = LZNT1 enabled
         * SMB2_COMPRESSION_LZ77_ENABLED (0x00000002) = LZ77 enabled
         * SMB2_COMPRESSION_LZ77_HUFFMAN_ENABLED (0x00000004) = LZ77+Huffman enabled
         * SMB2_COMPRESSION_PATTERN_V1_ENABLED (0x00000008) = Pattern Scanning enabled
         */
        rc_getint(rcfile, sname, "comp_algorithms_map", &prefs->compression_algorithms_map);
        
        rc_getint(rcfile, sname, "comp_io_threshold", &prefs->compression_io_threshold);
        /* Make sure they set it to something reasonable */
        if (prefs->compression_io_threshold < 4096) {
            /* 4096 bytes is the min since that is Windows Client min */
            prefs->compression_io_threshold = 4096;
        }
        
        if (prefs->compression_io_threshold > 5242880) {
            /* Set an arbitrary upper limit */
            prefs->compression_io_threshold = 5242880;
        }
        
        rc_getint(rcfile, sname, "comp_chunk_len", &prefs->compression_chunk_len);
        /* Make sure they set it to something reasonable */
        if (prefs->compression_chunk_len < 4096) {
            prefs->compression_chunk_len = 4096;
        }
        
        if (prefs->compression_chunk_len > 5242880) {
            /* Set an arbitrary upper limit */
            prefs->compression_chunk_len = 5242880;
        }

        rc_getint(rcfile, sname, "comp_max_fail_cnt", &prefs->compression_max_fail_cnt);
        /* Make sure they set it to something reasonable */
        if (prefs->compression_max_fail_cnt > 500000) {
            /* Set an arbitrary upper limit */
            prefs->compression_max_fail_cnt = 500000;
        }

        /* Check for a list of extensions to skip compression on */
        list_len = 0;
        rc_getstringptr(rcfile, sname, "comp_exclude_list", &str);
        if (str != NULL) {
            /* walk through tokens */
            token = strtok(str, ",");

            while ((token != NULL) && (list_len < kClientCompressMaxEntries)) {
                /* Sanity check the extension length */
                str_len = strlen(token);
                if (str_len > kClientCompressMaxExtLen) {
                    /* Too long, skip it */
                    os_log_error(OS_LOG_DEFAULT, "%s: Skipping exclude extension that is too long <%s>",
                                 __FUNCTION__, token);
                }
                else {
                    prefs->compression_exclude[list_len] = malloc(str_len + 1);
                    bzero(prefs->compression_exclude[list_len], (str_len + 1));
                    strncpy(prefs->compression_exclude[list_len], token, (str_len + 1));
                    list_len++;
                }

                token = strtok(NULL, ",");
            }

            prefs->compression_exclude_cnt = list_len;
        }

        /* Check for a list of extensions to allow compression on */
        list_len = 0;
        rc_getstringptr(rcfile, sname, "comp_include_list", &str);
        if (str != NULL) {
            /* walk through tokens */
            token = strtok(str, ",");

            while ((token != NULL) && (list_len < kClientCompressMaxEntries)) {
                /* Sanity check the extension length */
                str_len = strlen(token);
                if (str_len > kClientCompressMaxExtLen) {
                    /* Too long, skip it */
                    os_log_error(OS_LOG_DEFAULT, "%s: Skipping include extension that is too long <%s>",
                                 __FUNCTION__, token);
                }
                else {
                    prefs->compression_include[list_len] = malloc(str_len + 1);
                    bzero(prefs->compression_include[list_len], (str_len + 1));
                    strncpy(prefs->compression_include[list_len], token, (str_len + 1));
                    list_len++;
                }

                token = strtok(NULL, ",");
            }

            prefs->compression_include_cnt = list_len;
        }
        
        /* Change max channels */
        rc_getint(rcfile, sname, "mc_max_channels", (int32_t *) &prefs->mc_max_channels);
        /* Make sure they set it to something reasonable (in range 1-64) */
        if (prefs->mc_max_channels < 1) {
            prefs->mc_max_channels = 9; /* default is 8 active, 1 inactive */
        } else if (prefs->mc_max_channels > 64) {
            prefs->mc_max_channels = 64;
        }

        /* Change RSS server channels */
        rc_getint(rcfile, sname, "mc_srvr_rss_channels", (int32_t *) &prefs->mc_srvr_rss_channels);
        /* Make sure they set it to something reasonable (in range 1-64) */
        if (prefs->mc_srvr_rss_channels < 1) {
            prefs->mc_srvr_rss_channels = 4; // default is 4
        } else if (prefs->mc_srvr_rss_channels > 64) {
            prefs->mc_srvr_rss_channels = 64;
        }
        
        /* Change RSS client channels */
        rc_getint(rcfile, sname, "mc_clnt_rss_channels", (int32_t *) &prefs->mc_clnt_rss_channels);
        /* Make sure they set it to something reasonable (in range 1-64) */
        if (prefs->mc_clnt_rss_channels < 1) {
            prefs->mc_clnt_rss_channels = 4; // default is 4
        } else if (prefs->mc_clnt_rss_channels > 64) {
            prefs->mc_clnt_rss_channels = 64;
        }
	}
	
	/* global, server, user, or share preferences */
	
	/* Only get the value if it exist */
	if (rc_getbool(rcfile, sname, "compound_on", &altflags) == 0) {			
		if (altflags)
			prefs->altflags |= SMBFS_MNT_COMPOUND_ON;
		else
			prefs->altflags &= ~SMBFS_MNT_COMPOUND_ON;			
	}
	
	/* Only get the value if it exist */
	if (rc_getbool(rcfile, sname, "notify_off", &altflags) == 0) {			
		if (altflags)
			prefs->altflags |= SMBFS_MNT_NOTIFY_OFF;
		else
			prefs->altflags &= ~SMBFS_MNT_NOTIFY_OFF;			
	}
	
	/* Only get the value if it exist */
	if (rc_getbool(rcfile, sname, "streams", &altflags) == 0) {
		if (altflags)
			prefs->altflags |= SMBFS_MNT_STREAMS_ON;
		else
			prefs->altflags &= ~SMBFS_MNT_STREAMS_ON;			
	}
	
	/* Only get the value if it exist */
	if ( rc_getbool(rcfile, sname, "soft", &altflags) == 0) {
		if (altflags)
			prefs->altflags |= SMBFS_MNT_SOFT;
		else
			prefs->altflags &= ~SMBFS_MNT_SOFT;
	}
    
	/* Check for dir caching preferences */
    rc_getint(rcfile, sname, "dir_cache_async_cnt", &prefs->dir_cache_async_cnt);
    /* Make sure they set it to something reasonable */
    if (prefs->dir_cache_async_cnt > 100) {
        prefs->dir_cache_async_cnt = 100;
    }

    rc_getint(rcfile, sname, "dir_cache_max", &prefs->dir_cache_max);
    /* Make sure they set it to something reasonable */
    if (prefs->dir_cache_max > 3600) {
        prefs->dir_cache_max = 3600; /* 60 mins is a long, long time */
    }

    rc_getint(rcfile, sname, "dir_cache_min", &prefs->dir_cache_min);
    /* Make sure they set it to something reasonable */
    if (prefs->dir_cache_min < 1) {
        prefs->dir_cache_min = 1; /* 1s is the min */
    }

    rc_getint(rcfile, sname, "max_dirs_cached", &prefs->max_dirs_cached);
    /* Make sure they set it to something reasonable */
    if (prefs->max_dirs_cached > 5000) {
        prefs->max_dirs_cached = 5000; /* thats a lot of dirs! */
    }

    rc_getint(rcfile, sname, "max_cached_per_dir", &prefs->max_dir_entries_cached);
    /* Make sure they set it to something reasonable */
    if (prefs->max_dir_entries_cached > 500000) {
        prefs->max_dir_entries_cached = 500000; /* thats a lot of entries! */
    }

    if (rc_getbool(rcfile, sname, "submounts_off", &altflags) == 0) {
		if (altflags) {
			prefs->altflags |= SMBFS_MNT_SUBMOUNTS_OFF;
			os_log_debug(OS_LOG_DEFAULT, "%s: submounts disabled", __FUNCTION__);
		}
		else {
			prefs->altflags &= ~SMBFS_MNT_SUBMOUNTS_OFF;
		}
	}

    /*
     * Another config option to do NetBIOS name resolution before
     * attempting DNS name resolution. NetBIOS name resolution is slow so this
     * can cause a performance lag if the name is not a NetBIOS name.
     * Problem is that there are some DNS servers that respond to non existing
     * domain names with invalid IP addresses.
     */
    if (rc_getbool(rcfile, sname, "netBIOS_before_DNS", &altflags) == 0) {
        if (altflags) {
            prefs->try_netBIOS_before_DNS = 1;
            os_log_debug(OS_LOG_DEFAULT, "%s: Try NetBIOS before DNS resolution", __FUNCTION__);
        } else {
            prefs->try_netBIOS_before_DNS = 0;
        }
    }
    
    /*
     * Start of the HIDDEN options of nsmb.
     */
    
    /*
	 * We are not adding this in the man pages, because we do not want to keep
	 * this as a configuration option. This is for debug purposes only and 
	 * should be removed once <rdar://problem/7236779> is complete. Force
	 * ACLs on if we have a network sid.
	 */
	if (rc_getbool(rcfile, sname, "debug_acl_on", &altflags) == 0) {
		if (altflags)
			prefs->altflags |= SMBFS_MNT_DEBUG_ACL_ON;
		else
			prefs->altflags &= ~SMBFS_MNT_DEBUG_ACL_ON;			
	}
	
    /* 
	 * Another hidden config option. Force readdirattr off
	 */
	if (rc_getbool(rcfile, sname, "readdirattr_off", &altflags) == 0) {
		if (altflags)
			prefs->altflags |= SMBFS_MNT_READDIRATTR_OFF;
		else
			prefs->altflags &= ~SMBFS_MNT_READDIRATTR_OFF;			
	}

    /*
	 * Another hidden config option, to force LANMAN on
	 */
    if (rc_getbool(rcfile, sname, "lanman_on", &altflags) == 0) {
		if (altflags) {
			prefs->lanman_on = 1;
			os_log_debug(OS_LOG_DEFAULT, "%s: LANMAN support enabled", __FUNCTION__);
		} else {
			prefs->lanman_on = 0;
		}
	}

    /*
	 * Another hidden config option, to force Kerberos off
     * When <12991970> is fixed, remove this code
	 */
    if (rc_getbool(rcfile, sname, "kerberos_off", &altflags) == 0) {
		if (altflags) {
			prefs->altflags |= SMBFS_MNT_KERBEROS_OFF;
		} else {
			prefs->altflags &= ~SMBFS_MNT_KERBEROS_OFF;
		}
	}

    /*
	 * Another hidden config option, to force File IDs off
	 */
    if (rc_getbool(rcfile, sname, "file_ids_off", &altflags) == 0) {
		if (altflags) {
			prefs->altflags |= SMBFS_MNT_FILE_IDS_OFF;
		} else {
			prefs->altflags &= ~SMBFS_MNT_FILE_IDS_OFF;
		}
	}

    /*
	 * Another hidden config option, to force AAPL off
	 */
    if (rc_getbool(rcfile, sname, "aapl_off", &altflags) == 0) {
		if (altflags) {
			prefs->altflags |= SMBFS_MNT_AAPL_OFF;
		} else {
			prefs->altflags &= ~SMBFS_MNT_AAPL_OFF;
		}
	}
    
    /*
     * Another hidden config option, to disable matching on DNS names when
     * checking to see if a server is already mounted or not
     */
    if (rc_getbool(rcfile, sname, "no_DNS_match", &altflags) == 0) {
        if (altflags) {
            prefs->no_DNS_match = 1;
            os_log_debug(OS_LOG_DEFAULT, "%s: DNS name match disabled", __FUNCTION__);
        } else {
            prefs->no_DNS_match = 0;
        }
    }

	/*
	 * Another hidden config option, to turn off Dir Leasing
	 */
	if (rc_getbool(rcfile, sname, "dir_lease_off", &altflags) == 0) {
		if (altflags) {
			prefs->altflags |= SMBFS_MNT_DIR_LEASE_OFF;
		} else {
			prefs->altflags &= ~SMBFS_MNT_DIR_LEASE_OFF;
		}
	}
	
	/*
	 * Another hidden config option, to turn off File Leasing
	 */
	if (rc_getbool(rcfile, sname, "file_def_close_off", &altflags) == 0) {
		if (altflags) {
			prefs->altflags |= SMBFS_MNT_FILE_DEF_CLOSE_OFF;
		} else {
			prefs->altflags &= ~SMBFS_MNT_FILE_DEF_CLOSE_OFF;
		}
	}

	/*
	 * Another hidden config option, to turn off dir enumeration caching
	 */
	if (rc_getbool(rcfile, sname, "dir_cache_off", &altflags) == 0) {
		if (altflags) {
			prefs->altflags |= SMBFS_MNT_DIR_CACHE_OFF;
		} else {
			prefs->altflags &= ~SMBFS_MNT_DIR_CACHE_OFF;
		}
	}
    
    /*
     * Another hidden config options, to change quantum sizes and counts
     */
    rc_getint(rcfile, sname, "min_read_size", &prefs->read_size[0]);
    /* Make sure they set it to something reasonable. Match smb_maxread */
    if (prefs->read_size[0] > kDefaultMaxIOSize) {
        prefs->read_size[0] = kDefaultMaxIOSize;
    }
    
    rc_getint(rcfile, sname, "med_read_size", &prefs->read_size[1]);
    /* Make sure they set it to something reasonable. Match smb_maxread */
    if (prefs->read_size[1] > kDefaultMaxIOSize) {
        prefs->read_size[1] = kDefaultMaxIOSize;
    }
    
    rc_getint(rcfile, sname, "max_read_size", &prefs->read_size[2]);
    /* Make sure they set it to something reasonable. Match smb_maxread */
    if (prefs->read_size[2] > kDefaultMaxIOSize) {
        prefs->read_size[2] = kDefaultMaxIOSize;
    }

    rc_getint(rcfile, sname, "min_read_count", &prefs->read_count[0]);
    /* Make sure they set it to something reasonable. */
    if (prefs->read_count[0] > kQuantumNumberLimit) {
        prefs->read_count[0] = kQuantumNumberLimit;
    }
    
    rc_getint(rcfile, sname, "med_read_count", &prefs->read_count[1]);
    /* Make sure they set it to something reasonable. */
    if (prefs->read_count[1] > kQuantumNumberLimit) {
        prefs->read_count[1] = kQuantumNumberLimit;
    }
    
    rc_getint(rcfile, sname, "max_read_count", &prefs->read_count[2]);
    /* Make sure they set it to something reasonable. */
    if (prefs->read_count[2] > kQuantumNumberLimit) {
        prefs->read_count[2] = kQuantumNumberLimit;
    }
    
    /*
     * Write preferences
     */
    rc_getint(rcfile, sname, "min_write_size", &prefs->write_size[0]);
    /* Make sure they set it to something reasonable. Match smb_maxwrite */
    if (prefs->write_size[0] > kDefaultMaxIOSize) {
        prefs->write_size[0] = kDefaultMaxIOSize;
    }
    
    rc_getint(rcfile, sname, "med_write_size", &prefs->write_size[1]);
    /* Make sure they set it to something reasonable. Match smb_maxwrite */
    if (prefs->write_size[1] > kDefaultMaxIOSize) {
        prefs->write_size[1] = kDefaultMaxIOSize;
    }
    
    rc_getint(rcfile, sname, "max_write_size", &prefs->write_size[2]);
    /* Make sure they set it to something reasonable. Match smb_maxwrite */
    if (prefs->write_size[2] > kDefaultMaxIOSize) {
        prefs->write_size[2] = kDefaultMaxIOSize;
    }

    rc_getint(rcfile, sname, "min_write_count", &prefs->write_count[0]);
    /* Make sure they set it to something reasonable. */
    if (prefs->write_count[0] > kQuantumNumberLimit) {
        prefs->write_count[0] = kQuantumNumberLimit;
    }
    
    rc_getint(rcfile, sname, "med_write_count", &prefs->write_count[1]);
    /* Make sure they set it to something reasonable. */
    if (prefs->write_count[1] > kQuantumNumberLimit) {
        prefs->write_count[1] = kQuantumNumberLimit;
    }
    
    rc_getint(rcfile, sname, "max_write_count", &prefs->write_count[2]);
    /* Make sure they set it to something reasonable. */
    if (prefs->write_count[2] > kQuantumNumberLimit) {
        prefs->write_count[2] = kQuantumNumberLimit;
    }

    /*
     * Another hidden config option for how long to check read/write_count
     * quantum sizes/numbers. In microsecs
     */
    rc_getint(rcfile, sname, "rw_max_check_time", &prefs->rw_max_check_time);
    /* Make sure they set it to something reasonable < 1 min */
    if (prefs->rw_max_check_time > 60000000) {
        prefs->rw_max_check_time = 60000000;
    }

    /*
     * Another hidden config option for min GigBit speed to force a set
     * read/write Quantum size/number. In GigaBits
     */
    rc_getint(rcfile, sname, "rw_gb_threshold", &prefs->rw_gb_threshold);

    /* Another hidden config option to set IP QoS */
    rc_getint(rcfile, sname, "ip_qos", &prefs->ip_QoS);
    /* Make sure they set it to something reasonable */
    if (prefs->ip_QoS > 255) {
        os_log_error(OS_LOG_DEFAULT, "%s: Ignoring invalid QoS value <%d> ",
                     __FUNCTION__, prefs->ip_QoS);
        prefs->ip_QoS = 0;
    }
    
    /* Multichannel preferences */

    /*
     * Another hidden config option, to ignore client interfaces
     * expected list string format - "if_name_0,...,if_name_N"
     */
    rc_getstringptr(rcfile, sname, "mc_client_if_ignore_list", &str);

    if (str != NULL) {
        /* walk through tokens */
        token = strtok(str, ",");

        while ((token != NULL) && (ignorelist_len < kClientIfIgnorelistMaxLen)) {
            if_index = if_nametoindex(token);

            if (if_index) {
                prefs->mc_client_if_ignorelist[ignorelist_len] = if_index;
                ignorelist_len++;
            }

            token = strtok(NULL, ",");
        }

        prefs->mc_client_if_ignorelist_len = ignorelist_len;
    }
    
    /*
     * Another hidden config option, to force Client side RSS on
     */
    if (rc_getbool(rcfile, sname, "mc_client_rss_on", &altflags) == 0) {
        if (altflags) {
            prefs->altflags |= SMBFS_MNT_MC_CLIENT_RSS_FORCE_ON;
        } else {
            prefs->altflags &= ~SMBFS_MNT_MC_CLIENT_RSS_FORCE_ON;
        }
    }

    /*
     * Can disable SMB v3.1.1 if server is not doing pre auth integrity
     * check correctly.
     */
    if (rc_getbool(rcfile, sname, "disable_smb311", &altflags) == 0) {
        if (altflags)
            prefs->altflags |= SMBFS_MNT_DISABLE_311;
        else
            prefs->altflags &= ~SMBFS_MNT_DISABLE_311;
    }    

    /*
     * Another hidden config option, to not assume durable handle/lease V2
     */
    if (rc_getbool(rcfile, sname, "assume_dur_hndlV2_off", &altflags) == 0) {
        if (altflags) {
            prefs->altflags |= SMBFS_MNT_ASSUME_DUR_LEASE_V2_OFF;
        }
        else {
            prefs->altflags &= ~SMBFS_MNT_ASSUME_DUR_LEASE_V2_OFF;
        }
    }
    
    /*
     * Another hidden config option, to only use durable handles on O_EXLOCK
     * and O_SHLOCK opened files, essentially the previous OS behaviour
     */
    if (rc_getbool(rcfile, sname, "dur_handle_lockFID_only", &altflags) == 0) {
        if (altflags) {
            prefs->altflags |= SMBFS_MNT_DUR_HANDLE_LOCKFID_ONLY;
        }
        else {
            prefs->altflags &= ~SMBFS_MNT_DUR_HANDLE_LOCKFID_ONLY;
        }
    }
}

static CFStringRef getLocalNetBIOSNameUsingHostName(void)
{
	CFMutableStringRef NetBIOSName = NULL;
	char buf[_POSIX_HOST_NAME_MAX+1], *cp;
	
	if (gethostname(buf, sizeof(buf)) != 0) {
		os_log_debug(OS_LOG_DEFAULT, "%s: Couldn't obtain the Local NetBIOS Name using gethostname",
                     __FUNCTION__);
		return NULL;
	}
	cp = strchr(buf, '.');
	if (cp)
		*cp = 0;
	buf[MIN(SMB_MAXNetBIOSNAMELEN, _POSIX_HOST_NAME_MAX)] = 0;
	NetBIOSName = CFStringCreateMutable(kCFAllocatorSystemDefault, 0);
	if (NetBIOSName) {
		CFStringAppendCString(NetBIOSName, buf, kCFStringEncodingUTF8);
		CFStringUppercase(NetBIOSName, CFLocaleGetSystem());
	}
	return NetBIOSName;
}


/*
 * Retrieve any SMB System Configuration Preference. This routine always succeeds,
 * on any failure we fill in the default values.
 */
static void getSCPreferences(struct smb_prefs *prefs)
{
	SCPreferencesRef scPrefs;
	CFMutableStringRef NetBIOSName = NULL;
	CFStringRef DOSCodePage;
	
	scPrefs = SCPreferencesCreate(kCFAllocatorDefault, CFSTR("SMB Client"), CFSTR(kSMBPreferencesAppID));
	if (!scPrefs) {
		os_log_debug(OS_LOG_DEFAULT, "%s: Couldn't obtain system config preferences: %s",
                     __FUNCTION__, strerror(errno));
		prefs->WINSAddresses = NULL;
		prefs->WinCodePage = CFStringConvertWindowsCodepageToEncoding(defaultCodePage);
		goto done;
	}
	NetBIOSName = (CFMutableStringRef)SCPreferencesGetValue(scPrefs, CFSTR(kSMBPrefNetBIOSName));
	if (NetBIOSName) {
		NetBIOSName = CFStringCreateMutableCopy(kCFAllocatorSystemDefault, 0, NetBIOSName);
	}
	if (NetBIOSName) {
		CFStringUppercase(NetBIOSName, CFLocaleGetSystem());
	}
	prefs->LocalNetBIOSName = NetBIOSName;
	
	prefs->WINSAddresses = SCPreferencesGetValue(scPrefs, CFSTR(kSMBPrefWINSServerAddressList));
	if (prefs->WINSAddresses) {
		CFRetain(prefs->WINSAddresses);
#ifdef SMB_DEBUG
		char wins[SMB_MAX_DNS_SRVNAMELEN+1];
		CFIndex ii, count = CFArrayGetCount(prefs->WINSAddresses);

		for (ii=0; ii < count; ii++) {
			CFStringRef winString = CFArrayGetValueAtIndex(prefs->WINSAddresses, ii);
			wins[0] = 0;
			CFStringGetCString(winString, wins, sizeof(wins), kCFStringEncodingUTF8);
			os_log_error(OS_LOG_DEFAULT, "WINS[%d] \"%s\" ", (int)ii, wins);
		}
#endif // SMB_DEBUG		
	}
	DOSCodePage = SCPreferencesGetValue(scPrefs, CFSTR(kSMBPrefDOSCodePage));
	if (DOSCodePage) {
		if ((CFStringHasPrefix(DOSCodePage, CFSTR("CP")) == FALSE) && (CFStringHasPrefix(DOSCodePage, CFSTR("cp")) == FALSE)) {
			CFMutableStringRef WinCodePageStr = CFStringCreateMutableCopy(kCFAllocatorSystemDefault, 0, CFSTR("cp"));
			if (WinCodePageStr) {
				CFStringAppend(WinCodePageStr, DOSCodePage);
				prefs->WinCodePage = CFStringConvertIANACharSetNameToEncoding(WinCodePageStr);
				CFRelease(WinCodePageStr);
				goto done;
			}
		} else {
			prefs->WinCodePage = CFStringConvertIANACharSetNameToEncoding(DOSCodePage);
			goto done;
		}
	}
	/* We have no other choice use the default code page */
	prefs->WinCodePage = CFStringConvertWindowsCodepageToEncoding(defaultCodePage);
done:
	if (scPrefs) {
		CFRelease(scPrefs);
	}
	if (prefs->LocalNetBIOSName == NULL) {
		/* If all else fail try using the host name */
		prefs->LocalNetBIOSName = getLocalNetBIOSNameUsingHostName();
	}
	/* Test to make sure we have a code page we can use. */
	if (CFStringIsEncodingAvailable(prefs->WinCodePage) == FALSE) {
		prefs->WinCodePage = CFStringConvertWindowsCodepageToEncoding(defaultCodePage);
	}

}

void getDefaultPreferences(struct smb_prefs *prefs)
{
	 /* <11860141> Disable LANMAN (RAP) for getting share lists */
	memset(prefs, 0, sizeof(*prefs));
	prefs->tryBothPorts = TRUE;
	prefs->tcp_port = SMB_TCP_PORT_445;
	
    /* Dir caching is not implemented in deprecated readdirattr, so turn it off */	
	prefs->altflags =   SMBFS_MNT_STREAMS_ON |
                        SMBFS_MNT_COMPOUND_ON |
                        SMBFS_MNT_READDIRATTR_OFF |
                        SMBFS_MNT_MULTI_CHANNEL_ON;

    prefs->minAuthAllowed = SMB_MINAUTH_NTLMV2;
	prefs->NetBIOSResolverTimeout = DefaultNetBIOSResolverTimeout;
    
    prefs->dir_cache_async_cnt = 10; /* keep in sync with smb2fs_smb_cmpd_query_async */
    prefs->dir_cache_max = 60; /* Same as NFS */
    prefs->dir_cache_min = 30; /* Same as NFS */
    prefs->max_dirs_cached = 0; /* Use defaults */
    prefs->max_dir_entries_cached = 0; /* Use defaults */

	/* Signing required is OFF by default */
	prefs->signing_required = 0;
    /* all signing algorithms are enabled by default */
    prefs->signing_algorithm_map = 0xf;
    /*
     * If signing_required is enabled, then signing is required for SMB 2/3,
     * but not for SMB 1 by default
     */
	prefs->signing_req_versions = 6;

	/* SMB 1/2/3 enabled by default */
	prefs->protocol_version_map = 7;

    /* IP QoS of 0 means use the default */
    prefs->ip_QoS = 0;

    /* quantum size or count of 0 means use the default */
    prefs->read_size[0] = 0;
    prefs->read_size[1] = 0;
    prefs->read_size[2] = 0;
    prefs->read_count[0] = 0;
    prefs->read_count[1] = 0;
    prefs->read_count[2] = 0;

    prefs->write_size[0] = 0;
    prefs->write_size[1] = 0;
    prefs->write_size[2] = 0;
    prefs->write_count[0] = 0;
    prefs->write_count[1] = 0;
    prefs->write_count[2] = 0;

    /* rw_max_check_time */
    prefs->rw_max_check_time = 0;
    
    /* rw_gb_threshold of 0 means use the default behavior */
    prefs->rw_gb_threshold = 0;

    /* multichannel defaults */
    prefs->mc_max_channels = 9;     /* 8 active, 1 inactive */
    prefs->mc_srvr_rss_channels = 4;
    prefs->mc_clnt_rss_channels = 4;
    prefs->mc_client_if_ignorelist_len = 0;

    /* AES_128_CCM/AES_128_GCM/AES-256-CCM/AES-256-GCM enabled by default */
    prefs->encrypt_algorithm_map = 0xf;

    /* Force session level encryption is OFF by default */
    prefs->force_sess_encrypt = 0;

    /* Force share level encryption is OFF by default */
    prefs->force_share_encrypt = 0;

    /*
     * Compression
     */
    prefs->compression_algorithms_map = 0x0; /* Off by default for now */
    prefs->compression_io_threshold = 4096;
    prefs->compression_chunk_len = 262144;
    prefs->compression_max_fail_cnt = 5;

    /* Now get any values stored in the System Configuration */
    getSCPreferences(prefs);
    
}

void releasePreferenceInfo(struct smb_prefs *prefs)
{
    uint32_t i = 0;
    
	if (prefs->LocalNetBIOSName) {
		CFRelease(prefs->LocalNetBIOSName);
		prefs->LocalNetBIOSName = NULL;
	}
	if (prefs->WINSAddresses) {
		CFRelease(prefs->WINSAddresses);
		prefs->WINSAddresses = NULL;
	}
	if (prefs->NetBIOSDNSName) {
		CFRelease(prefs->NetBIOSDNSName);
		prefs->NetBIOSDNSName = NULL;
	}
    
    for (i = 0; i < prefs->compression_exclude_cnt; i++) {
        if (prefs->compression_exclude[i] != NULL) {
            free(prefs->compression_exclude[i]);
        }
    }
    prefs->compression_exclude_cnt = 0;
    
    for (i = 0; i < prefs->compression_include_cnt; i++) {
        if (prefs->compression_include[i] != NULL) {
            free(prefs->compression_include[i]);
        }
    }
    prefs->compression_include_cnt = 0;
}

void setWINSAddress(struct smb_prefs *prefs, const char *winsAddress, int count)
{
	int ii;
	CFMutableArrayRef winsArray = CFArrayCreateMutable( kCFAllocatorSystemDefault, 0, 
													   &kCFTypeArrayCallBacks );
	if (winsArray == NULL) {
		return;
	}
	for (ii=0; ii < count; ii++) {
		CFStringRef winsName = CFStringCreateWithCString(kCFAllocatorSystemDefault, winsAddress, kCFStringEncodingUTF8);
		if (winsName) {
			CFArrayAppendValue(winsArray, winsName);
			CFRelease(winsName);
		}
		winsAddress += strlen(winsAddress) + 1;
	}
	if (CFArrayGetCount(winsArray) == 0) {
		CFRelease(winsArray);
	}
	if (prefs->WINSAddresses) {
		CFRelease(prefs->WINSAddresses);
	}
	prefs->WINSAddresses = winsArray;
}

void readPreferences(struct smb_prefs *prefs, char *serverName, char *shareName, 
					 int noUserPrefs, int resetPrefs)
{
	struct rcfile *rcfile;
	char sname[SMB_MAX_DNS_SRVNAMELEN + SMB_MAXSHARENAMELEN + 4];
	
	/* Set the default values */
	if (resetPrefs) {
		getDefaultPreferences(prefs);
	}

	/* Now read the nsmb.conf file preference */
	rcfile = smb_open_rcfile(noUserPrefs);
	if (rcfile == NULL) {
		return;
	}
	/* Read the global preference section */
	readPreferenceSection(rcfile, prefs, "default", 0);
	
	/* Need a server name, before we can read any of the [server] sections. */
	if (serverName) {
		readPreferenceSection(rcfile, prefs, serverName, 1);
		/* Need a server and share name, before we can read any of the [server:share] sections. */
		if (shareName) {
			snprintf(sname, sizeof(sname), "%s:%s", serverName, shareName);
			readPreferenceSection(rcfile, prefs, sname, 2);
		}
	}
	/* Done with it close the preference file */
	rc_close(rcfile);
}


CFStringEncoding getPrefsCodePage( void )
{
	struct smb_prefs prefs;
	
	getDefaultPreferences(&prefs);
	releasePreferenceInfo(&prefs);
	return prefs.WinCodePage;
}
