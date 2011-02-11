# This file is part of mingw-cross-env.
# See doc/index.html for further information.

# aspell
PKG             := aspell
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := 0.60.6
$(PKG)_CHECKSUM := 335bcb560e00f59d89ec9e4c4114c325fb0e65f4
$(PKG)_SUBDIR   := aspell-$($(PKG)_VERSION)
$(PKG)_FILE     := aspell-$($(PKG)_VERSION).tar.gz
$(PKG)_WEBSITE  := http://aspell.net
$(PKG)_URL      := ftp://ftp.gnu.org/gnu/aspell/$($(PKG)_FILE)
$(PKG)_DEPS     := gcc gettext pthreads

define $(PKG)_UPDATE
    wget -q -O- 'http://aspell.net/' | \
    grep 'aspell-' | \
    $(SED) -n 's,.*aspell-\([0-9][^>]*\)\.tar.*,\1,p' | \
    head -1
endef

define $(PKG)_BUILD
    cd '$(1)' && ./configure \
        --host='$(TARGET)' \
        --disable-shared \
        --prefix='$(PREFIX)/$(TARGET)' \
        --disable-rpath \
	--enable-win32-relocatable \
        CONFIG_SHELL=$(SHELL)
    $(MAKE) -C '$(1)' -j '$(JOBS)' install
endef
