.PATH: ${.CURDIR}/posix1e

MISRCS+= acl.c				\
	 acl_entry.c			\
	 acl_file.c			\
	 acl_flag.c			\
	 acl_perm.c			\
	 acl_translate.c		

.if ${LIB} == "c"

MAN3 +=	acl.3				\
	acl_add_flag_np.3		\
	acl_add_perm.3			\
	acl_clear_flags_np.3		\
	acl_clear_perms.3		\
	acl_copy_entry.3		\
	acl_create_entry.3		\
	acl_delete.3			\
	acl_delete_entry.3		\
	acl_delete_flag_np.3		\
	acl_delete_perm.3		\
	acl_dup.3			\
	acl_free.3			\
	acl_from_text.3			\
	acl_get.3			\
	acl_get_entry.3			\
	acl_get_flagset_np.3		\
	acl_get_permset.3		\
	acl_get_perm_np.3		\
	acl_get_qualifier.3		\
	acl_get_tag_type.3		\
	acl_init.3			\
	acl_set.3			\
	acl_set_flagset_np.3		\
	acl_set_permset.3		\
	acl_set_qualifier.3		\
	acl_set_tag_type.3		\
	acl_to_text.3			\
	acl_valid.3

MLINKS+=acl_create_entry.3 acl_create_entry_np.3 \
	acl_delete.3 acl_delete_file_np.3	\
	acl_delete.3 acl_delete_fd_np.3		\
	acl_get.3 acl_get_file.3		\
	acl_get.3 acl_get_fd.3			\
	acl_get.3 acl_get_fd_np.3		\
	acl_set.3 acl_set_file.3		\
	acl_set.3 acl_set_fd.3			\
	acl_set.3 acl_set_fd_np.3		\
	acl_valid.3 acl_valid_file_np.3		\
	acl_valid.3 acl_valid_fd_np.3

.endif
