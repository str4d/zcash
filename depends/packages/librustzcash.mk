package=librustzcash
$(package)_version=0.1
$(package)_download_path=https://github.com/str4d/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_git_commit).tar.gz
$(package)_download_file=$($(package)_git_commit).tar.gz
$(package)_sha256_hash=7935964752d564b412fa72aeb5e55268348f761eeb3c219589a11d3db38a8efc
$(package)_git_commit=8caaac01dbfa0b9207393a5f4683fc87cb06daa9
$(package)_dependencies=rust $(rust_crates)
$(package)_patches=cargo.config remove-dev-dependencies.diff

$(package)_rust_target=$(if $(rust_rust_target_$(canonical_host)),$(rust_rust_target_$(canonical_host)),$(canonical_host))

ifeq ($(host_os),mingw32)
$(package)_library_file=target/x86_64-pc-windows-gnu/release/rustzcash.lib
else ifneq ($(canonical_host),$(build))
$(package)_library_file=target/$($(package)_rust_target)/release/librustzcash.a
else
$(package)_library_file=target/release/librustzcash.a
endif

define $(package)_set_vars
$(package)_build_opts=--frozen --release
ifneq ($(canonical_host),$(build))
$(package)_build_opts+=--target=$($(package)_rust_target)
endif
endef

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/remove-dev-dependencies.diff && \
  mkdir .cargo && \
  cat $($(package)_patch_dir)/cargo.config | sed 's|CRATE_REGISTRY|$(host_prefix)/$(CRATE_REGISTRY)|' > .cargo/config
endef

define $(package)_build_cmds
  $(host_prefix)/native/bin/cargo build --package librustzcash $($(package)_build_opts)
endef

define $(package)_stage_cmds
  mkdir $($(package)_staging_dir)$(host_prefix)/lib/ && \
  mkdir $($(package)_staging_dir)$(host_prefix)/include/ && \
  cp $($(package)_library_file) $($(package)_staging_dir)$(host_prefix)/lib/ && \
  cp librustzcash/include/librustzcash.h $($(package)_staging_dir)$(host_prefix)/include/
endef
