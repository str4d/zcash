package=tor
$(package)_version=0.3.2.10
$(package)_download_path=https://www.torproject.org/dist/
$(package)_file_name=tor-$($(package)_version).tar.gz
$(package)_sha256_hash=60df77c31dcf94fdd686c8ca8c34f3b70243b33a7344ecc0b719d5ca2617cbee
$(package)_dependencies=libevent openssl zlib
$(package)_config_opts=--enable-static-libevent --enable-static-openssl --enable-static-zlib

define $(package)_config_cmds
  $($(package)_autoconf) --host=$(host) --build=$(build) --with-libevent-dir=$(host_prefix) --with-openssl-dir=$(host_prefix) --with-zlib-dir=$(host_prefix)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install ; echo '=== staging find for $(package):' ; find $($(package)_staging_dir)
endef
