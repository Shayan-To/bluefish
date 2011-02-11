# This file is part of mingw-cross-env.
# See doc/index.html for further information.

# gettext
PKG             := bluefish
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := 2.0.3-rc2
$(PKG)_CHECKSUM := 2877cb3387472fb86b15ea622c076d09a52155ad
$(PKG)_SUBDIR   := bluefish-$($(PKG)_VERSION)
$(PKG)_FILE     := bluefish-$($(PKG)_VERSION).tar.bz2
$(PKG)_WEBSITE  := http://bluefish.openoffice.nl
$(PKG)_URL      := http://www.bennewitz.com/bluefish/stable/source/$($(PKG)_FILE)
$(PKG)_DEPS     := gcc gettext glib gtk libenchant libgucharmap libtool pango libxml2 zlib

define $(PKG)_UPDATE
    wget -q -O- 'http://www.gnu.org/software/gettext/' | \
    grep 'gettext-' | \
    $(SED) -n 's,.*gettext-\([0-9][^>]*\)\.tar.*,\1,p' | \
    head -1
endef

define $(PKG)_BUILD
    cd '$(1)' && ./configure \
        --host='$(TARGET)' \
        --disable-static \
        --disable-xml-catalog-update \
        --disable-update-databases \
        --prefix='$(PREFIX)/$(TARGET)' \
        --sysconfdir='$(PREFIX)/$(TARGET)/etc' \
        --with-icon-path='$(PREFIX)/$(TARGET)/share/pixmaps' \
        --without-theme-path \
        --without-freedesktop_org-menu \
        --without-freedesktop_org-mime \
        --without-xml-catalog \
        CONFIG_SHELL=$(SHELL) \
        PKG_CONFIG='$(PREFIX)/bin/$(TARGET)-pkg-config' \
        CFLAGS="-g -Wall -O2 -mwindows" \
        CPPFLAGS="-g -Wall -O2" \
        LDFLAGS="-Wl,--as-needed -Wl,-O1"
    $(MAKE) -C '$(1)' -j '$(JOBS)' install
endef
