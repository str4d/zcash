package=crate_tracing_attributes
$(package)_crate_name=tracing-attributes
$(package)_version=0.1.8
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=99bbad0de3fd923c9c3232ead88510b783e5a4d16a6154adffa3d53308de984c
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
