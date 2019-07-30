package=crate_getrandom
$(package)_crate_name=getrandom
$(package)_version=0.1.8
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=34f33de6f0ae7c9cb5e574502a562e2b512799e32abb801cd1e79ad952b62b49
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
