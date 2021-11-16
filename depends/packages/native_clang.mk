package=native_clang
$(package)_major_version=13
$(package)_version=13.0.1
$(package)_download_path=https://github.com/llvm/llvm-project/releases/download/llvmorg-$($(package)_version)
$(package)_download_path_linux=https://github.com/llvm/llvm-project/releases/download/llvmorg-$($(package)_version)
$(package)_download_file_linux=clang+llvm-$($(package)_version)-x86_64-linux-gnu-ubuntu-18.04.tar.xz
$(package)_file_name_linux=clang-llvm-$($(package)_version)-x86_64-linux-gnu-ubuntu-18.04.tar.xz
$(package)_sha256_hash_linux=84a54c69781ad90615d1b0276a83ff87daaeded99fbc64457c350679df7b4ff0
$(package)_download_path_darwin=https://github.com/llvm/llvm-project/releases/download/llvmorg-$($(package)_version)
$(package)_download_file_darwin=clang+llvm-$($(package)_version)-x86_64-apple-darwin.tar.xz
$(package)_file_name_darwin=clang-llvm-$($(package)_version)-x86_64-apple-darwin.tar.xz
$(package)_sha256_hash_darwin=dec02d17698514d0fc7ace8869c38937851c542b02adf102c4e898f027145a4d
$(package)_download_path_freebsd=https://github.com/llvm/llvm-project/releases/download/llvmorg-$($(package)_version)
$(package)_download_file_freebsd=clang+llvm-$($(package)_version)-amd64-unknown-freebsd12.tar.xz
$(package)_file_name_freebsd=clang-llvm-$($(package)_version)-amd64-unknown-freebsd12.tar.xz
$(package)_sha256_hash_freebsd=8101c8d3a920bf930b33987ada5373f43537c5de8c194be0ea10530fd0ad5617
$(package)_download_file_aarch64_linux=clang+llvm-$($(package)_version)-aarch64-linux-gnu.tar.xz
$(package)_file_name_aarch64_linux=clang-llvm-$($(package)_version)-aarch64-linux-gnu.tar.xz
$(package)_sha256_hash_aarch64_linux=15ff2db12683e69e552b6668f7ca49edaa01ce32cb1cbc8f8ed2e887ab291069
$(package)_download_path_aarch64_darwin=https://ghcr.io/v2/homebrew/core/llvm/blobs
$(package)_download_file_aarch64_darwin=sha256:67dfef6403fd3cdcd099862e67e88839e3783f37ab994e10f7c07df8324b3f54
$(package)_file_name_aarch64_darwin=clang-llvm-$($(package)_version)-aarch64-apple-darwin.tar.xz
$(package)_sha256_hash_aarch64_darwin=67dfef6403fd3cdcd099862e67e88839e3783f37ab994e10f7c07df8324b3f54

ifneq (,$(wildcard /etc/arch-release))
$(package)_dependencies=native_libtinfo
endif

# Ensure we have clang native to the builder, not the target host
ifneq ($(canonical_host),$(build))
$(package)_exact_download_path=$($(package)_download_path_$(build_os))
$(package)_exact_download_file=$($(package)_download_file_$(build_os))
$(package)_exact_file_name=$($(package)_file_name_$(build_os))
$(package)_exact_sha256_hash=$($(package)_sha256_hash_$(build_os))
endif

# Homebrew places its packages in a versioned subdir
ifeq ($(build_arch)_$(build_os),aarch64_darwin)
$(package)_archive_root=$($(package)_version)_1
else
$(package)_archive_root=.
endif

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/bin && \
  cp $($(package)_archive_root)/bin/clang-$($(package)_major_version) $($(package)_staging_prefix_dir)/bin && \
  cp $($(package)_archive_root)/bin/lld $($(package)_staging_prefix_dir)/bin && \
  cp $($(package)_archive_root)/bin/llvm-ar $($(package)_staging_prefix_dir)/bin && \
  cp $($(package)_archive_root)/bin/llvm-config $($(package)_staging_prefix_dir)/bin && \
  cp $($(package)_archive_root)/bin/llvm-nm $($(package)_staging_prefix_dir)/bin && \
  cp $($(package)_archive_root)/bin/llvm-objcopy $($(package)_staging_prefix_dir)/bin && \
  cp -P $($(package)_archive_root)/bin/clang $($(package)_staging_prefix_dir)/bin && \
  cp -P $($(package)_archive_root)/bin/clang++ $($(package)_staging_prefix_dir)/bin && \
  cp -P $($(package)_archive_root)/bin/ld.lld $($(package)_staging_prefix_dir)/bin && \
  cp -P $($(package)_archive_root)/bin/ld64.lld $($(package)_staging_prefix_dir)/bin && \
  cp -P $($(package)_archive_root)/bin/lld-link $($(package)_staging_prefix_dir)/bin && \
  cp -P $($(package)_archive_root)/bin/llvm-ranlib $($(package)_staging_prefix_dir)/bin && \
  cp -P $($(package)_archive_root)/bin/llvm-strip $($(package)_staging_prefix_dir)/bin && \
  mv $($(package)_archive_root)/include/ $($(package)_staging_prefix_dir) && \
  mv $($(package)_archive_root)/lib/ $($(package)_staging_prefix_dir) && \
  mv $($(package)_archive_root)/libexec/ $($(package)_staging_prefix_dir)
endef
