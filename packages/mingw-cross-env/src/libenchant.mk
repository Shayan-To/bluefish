# This file is part of mingw-cross-env.
# See doc/index.html for further information.

# libenchant by abisource
PKG             := libenchant
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := 1.6.0
$(PKG)_CHECKSUM := 321f9cf0abfa1937401676ce60976d8779c39536
$(PKG)_SUBDIR   := enchant-$($(PKG)_VERSION)
$(PKG)_FILE     := enchant-$($(PKG)_VERSION).tar.gz
$(PKG)_WEBSITE  := http://www.abisource.com/projects/enchant/
$(PKG)_URL      := http://www.abisource.com/downloads/enchant/$($(PKG)_VERSION)/$($(PKG)_FILE)
$(PKG)_DEPS     := gcc pthreads glib zlib

define $(PKG)_UPDATE
    wget -q -O- 'http://www.abisource.com/projects/enchant/' | \
    grep 'enchant-' | \
    $(SED) -n 's,.*enchant-\([0-9][^>]*\)\.tar.*,\1,p' | \
    head -1
endef

define $(PKG)_BUILD
    $(SED) -i 's,\$${WINDRES-windres},$(TARGET)-windres,' '$(1)/compile-resource'
    cd '$(1)' && ./configure \
        --host='$(TARGET)' \
	--target='$(TARGET)' \
        --disable-shared \
        --disable-ispell \
        --disable-myspell \
        --prefix='$(PREFIX)/$(TARGET)' \
        CONFIG_SHELL=$(SHELL) \
        PKG_CONFIG='$(PREFIX)/bin/$(TARGET)-pkg-config'
    $(MAKE) -C '$(1)' -j '$(JOBS)' install sbin_PROGRAMS= bin_PROGRAMS= noinst_PROGRAMS=
endef
