package=crate_blake2b_simd
$(package)_crate_name=blake2b_simd
$(package)_version=0.5.6
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=461f4b879a8eb70c1debf7d0788a9a5ff15f1ea9d25925fea264ef4258bed6b2
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
