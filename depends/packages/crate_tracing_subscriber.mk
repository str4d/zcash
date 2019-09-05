package=crate_tracing_subscriber
$(package)_crate_name=tracing-subscriber
$(package)_version=0.2.7
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=c72c8cf3ec4ed69fef614d011a5ae4274537a8a8c59133558029bd731eb71659
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
