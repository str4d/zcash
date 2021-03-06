package=native_cxxbridge
$(package)_version=1.0.49
$(package)_download_path=https://github.com/dtolnay/cxx/archive
$(package)_file_name=native_cxxbridge-$($(package)_version).tar.gz
$(package)_download_file=$($(package)_version).tar.gz
$(package)_sha256_hash=ec47c18cffaedb56f31c9631e48950ac7f39012d0a4f9cc38536e74325a5dd99
$(package)_build_subdir=gen/cmd
$(package)_dependencies=native_rust

# TODO: Download and vendor dependencies.

define $(package)_build_cmds
  cargo build --release
endef

define $(package)_stage_cmds
  mkdir -p "$($(package)_staging_prefix_dir)" && \
  cargo install --path=. --root=$($(package)_staging_prefix_dir)
endef
