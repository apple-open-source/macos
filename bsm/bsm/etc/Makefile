BSM_ETC_OPEN_FILES=audit_class audit_event
BSM_ETC_RESTRICTED_FILES=audit_control audit_user
BSM_ETC_EXEC_FILES=audit_warn
BSM_ETC_FILES=$(BSM_ETC_OPEN_FILES) $(BSM_ETC_RESTRICTED_FILES) $(BSM_ETC_EXEC_FILES)
BSM_ETC_DIR=/etc/security

all: build

build:
	@echo null etc build.

install:
	@echo Not installing $(BSM_ETC_DIR) configuration files automatically

clean:
	@echo null etc clean.

first-install:
	/bin/mkdir -p $(BSM_ETC_DIR)
	/bin/cp ${BSM_ETC_FILES} $(BSM_ETC_DIR)
	cd $(BSM_ETC_DIR)
	/bin/chmod 0444 $(BSM_ETC_OPEN_FILES)
	/bin/chmod 0400 $(BSM_ETC_RESTRICTED_FILES)
	/bin/chmod 0555 $(BSM_ETC_EXEC_FILES)
