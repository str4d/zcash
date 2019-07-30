package=crate_lazy_static
$(package)_crate_name=lazy_static
$(package)_version=1.3.0
$(package)_download_path=https://static.crates.io/crates/$($(package)_crate_name)
$(package)_file_name=$($(package)_crate_name)-$($(package)_version).crate
$(package)_sha256_hash=bc5729f27f159ddd61f4df6228e827e86643d4d3e7c32183cb30a1c08f604a14
$(package)_crate_versioned_name=$($(package)_crate_name)

define $(package)_preprocess_cmds
  $(call generate_crate_checksum,$(package))
endef

define $(package)_stage_cmds
  $(call vendor_crate_source,$(package))
endef
