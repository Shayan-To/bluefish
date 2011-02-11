# This file is part of mingw-cross-env.
# See doc/index.html for further information.

# regex
PKG             := libgnurx
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := 2.5
$(PKG)_CHECKSUM := 321f9cf0abfa1937401676ce60976d8779c39536
$(PKG)_SUBDIR   := libgnurx-src-$($(PKG)_VERSION)
$(PKG)_FILE     := libgnurx-src-$($(PKG)_VERSION).zip
$(PKG)_WEBSITE  := http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/
$(PKG)_URL      := http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/$($(PKG)_FILE)
$(PKG)_DEPS     := gcc

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
