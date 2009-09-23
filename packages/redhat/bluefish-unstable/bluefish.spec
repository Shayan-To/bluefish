%define name	bluefish-unstable
%define version	1.3.7
%define release	5
%define distro	fc11
%define source	bluefish-unstable-1.3.7


Summary: A GTK2 web development application for experienced users
Name: %{name}
Version: %{version}
Release: %{release}.%{distro}
Source: ftp://ftp.ratisbona.com/pub/bluefish/snapshots/%{source}.tar.gz
URL: http://bluefish.openoffice.nl
License: GPL
Group: Development/Tools
Requires: gtk2, pcre, aspell
BuildRequires: gtk2-devel, pcre-devel, aspell-devel
BuildRequires: desktop-file-utils, gettext, libxml2, perl-XML-Parser
BuildRequires: enchant-devel
Requires(post): desktop-file-utils, shared-mime-info
Requires(postun): desktop-file-utils, shared-mime-info
BuildRoot: %{_tmppath}/%{name}-%{release}-root

%description
Bluefish is a powerful editor for experienced web designers and programmers.
Bluefish supports many programming and markup languages, but it focuses on
editing dynamic and interactive websites

%prep
%setup -q -n %{source}

%build
%configure --disable-update-databases \
  --disable-xml-catalog-update        \
  --without-gnome2_4-mime             \
  --without-gnome2_4-appreg           

%{__make} %{?_smp_mflags}

%install
%{__rm} -rf %{buildroot}
make install DESTDIR=%{buildroot}
%{__rm} -f %{buildroot}%{_libdir}/%{name}/*.*a

# required ugly joining
%find_lang %{name}
%find_lang %{name}_plugin_about
%find_lang %{name}_plugin_entities
%find_lang %{name}_plugin_infbrowser
%find_lang %{name}_plugin_htmlbar
%find_lang %{name}_plugin_snippets
%find_lang %{name}_plugin_charmap
%{__cat} %{name}_plugin_about.lang >> %{name}.lang
%{__cat} %{name}_plugin_entities.lang >> %{name}.lang
%{__cat} %{name}_plugin_infbrowser.lang >> %{name}.lang
%{__cat} %{name}_plugin_htmlbar.lang >> %{name}.lang
%{__cat} %{name}_plugin_snippets.lang >> %{name}.lang
%{__cat} %{name}_plugin_charmap.lang >> %{name}.lang

desktop-file-install --vendor=fedora --delete-original \
  --dir %{buildroot}%{_datadir}/applications           \
  --add-category X-Fedora                              \
  --add-category Development                           \
  %{buildroot}%{_datadir}/applications/%{name}.desktop

%clean
%{__rm} -rf %{buildroot}

%post
update-mime-database %{_datadir}/mime > /dev/null 2>&1 || :
update-desktop-database %{_datadir}/applications > /dev/null 2>&1 || :
xmlcatalog --noout --add 'public' 'Bluefish/DTD/Bflang' 'bflang.dtd'     \
  /etc/xml/catalog
xmlcatalog --noout --add 'system'                                        \
  'http://bluefish.openoffice.nl/DTD/bflang.dtd' 'bflang.dtd'            \
  /etc/xml/catalog
xmlcatalog --noout --add 'rewriteURI'                                    \
  'http://bluefish.openoffice.nl/DTD' '/usr/share/xml/bluefish-unstable' \
  /etc/xml/catalog

%postun
update-mime-database %{_datadir}/mime > /dev/null 2>&1 || :
update-desktop-database %{_datadir}/applications > /dev/null 2>&1 || :
xmlcatalog --noout --del 'bflang.dtd' /etc/xml/catalog
xmlcatalog --noout --del 'http://bluefish.openoffice.nl/DTD' /etc/xml/catalog

%files -f %{name}.lang
%defattr(-, root, root)
%doc AUTHORS COPYING README TODO
%{_bindir}/*
%{_datadir}/%{name}
%{_libdir}/%{name}
%{_datadir}/applications/*
%{_datadir}/mime/packages/*
%{_datadir}/pixmaps/*
%{_datadir}/icons/hicolor/*/*/*
%{_mandir}/man1/*



%changelog
* Wed Sep 23 2009 Matthias Haase <matthias_haase@bennewitz.com> - 1.3.7-5.fc11
- Automatic build
