package=i2pd
$(package)_version=2.29.0
$(package)_download_path=https://github.com/PurpleI2P/$(package)/archive
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_download_file=$($(package)_version).tar.gz
$(package)_sha256_hash=fd0474c33b411593b9dc8197f3799d37d68455c11a9ee3994ec993a96388ec06
$(package)_dependencies=boost openssl zlib
$(package)_patches=header-namespace.diff

define $(package)_set_vars
  $(package)_config_opts= CC="$($(package)_cc)"
  $(package)_config_opts+=CXX="$($(package)_cxx)"
  $(package)_config_opts+=AR="$($(package)_ar)"
  $(package)_config_opts+=CXXFLAGS="$($(package)_cxxflags) -I$($($(package)_type)_prefix)/include"
  $(package)_config_opts+=LIBDIR="$($($(package)_type)_prefix)/lib"
  $(package)_config_opts+=USE_STATIC=yes
endef

define $(package)_build_cmds
  $(AT)mkdir -p obj && \
  $(AT)mkdir -p obj/libi2pd && \
  $(MAKE) $($(package)_config_opts) api
endef

define $(package)_stage_cmds
  patch -p1 <$($(package)_patch_dir)/header-namespace.diff && \
  mkdir -p $($(package)_staging_dir)$(host_prefix)/lib && \
  mkdir -p $($(package)_staging_dir)$(host_prefix)/include/libi2pd && \
  install ./libi2pd.a $($(package)_staging_dir)$(host_prefix)/lib/libi2pd.a && \
  cp -a ./libi2pd/*.hpp $($(package)_staging_dir)$(host_prefix)/include/libi2pd && \
  cp -a ./libi2pd/*.h $($(package)_staging_dir)$(host_prefix)/include/libi2pd
endef
