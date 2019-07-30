package=crate_blake2s_simd
$(package)_crate_name=blake2s_simd
$(package)_version=0.5.6
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=3a84d2614b18a5367d357331a90fd533d5ceb1e86abc319320df2104ab744c2a
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
