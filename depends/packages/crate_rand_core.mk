package=crate_rand_core
$(package)_crate_name=rand_core
$(package)_version=0.5.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=615e683324e75af5d43d8f7a39ffe3ee4a9dc42c5c701167a71dc59c3a493aca
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
