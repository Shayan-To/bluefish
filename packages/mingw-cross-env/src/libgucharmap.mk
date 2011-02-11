# This file is part of mingw-cross-env.
# See doc/index.html for further information.

# gucharmap
PKG             := libgucharmap
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := 2.32.1
$(PKG)_CHECKSUM := 4cb86f114e4ed2921fb4ffa58caa0370e4f82f0c
$(PKG)_SUBDIR   := gucharmap-$($(PKG)_VERSION)
$(PKG)_FILE     := gucharmap-$($(PKG)_VERSION).tar.bz2
$(PKG)_WEBSITE  := http://live.gnome.org/Gucharmap
$(PKG)_URL      := http://ftp.gnome.org/pub/GNOME/sources/gucharmap/$(call SHORT_PKG_VERSION,$(PKG))/$($(PKG)_FILE)
$(PKG)_DEPS     := gcc gettext glib gtk popt

define $(PKG)_BUILD
    cd '$(1)' && ./configure \
        --host='$(TARGET)' \
        --disable-shared \
        --disable-gconf \
        --disable-schemas-install \
        --disable-scrollkeeper \
        --disable-python-bindings \
        --disable-introspection \
        --prefix='$(PREFIX)/$(TARGET)' \
        CONFIG_SHELL=$(SHELL) \
        PKG_CONFIG='$(PREFIX)/bin/$(TARGET)-pkg-config' \
        GLIB_GENMARSHAL='$(PREFIX)/$(TARGET)/bin/glib-genmarshal' \
        GLIB_MKENUMS='$(PREFIX)/$(TARGET)/bin/glib-mkenums'
    $(MAKE) -C '$(1)' -j '$(JOBS)' install sbin_PROGRAMS= bin_PROGRAMS= noinst_PROGRAMS=
endef
