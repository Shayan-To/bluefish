%define name	bluefish
%define version	2.2.4
%define release	3
%define distro	fc18
%define source	bluefish-2.2.4

Summary: A GTK2 web development application for experienced users
Name: %{name}
Version: %{version}
Release: %{release}.%{distro}
URL: http://bluefish.openoffice.nl/
Source: http://www.bennewitz.com/bluefish/stable/source/%{source}.tar.gz
License: GPLv2+
Group: Development/Tools
Requires: gtk2, findutils, grep
BuildRequires: glib2-devel, gtk2-devel
BuildRequires: desktop-file-utils, gettext, libxml2, perl-XML-Parser
BuildRequires: enchant-devel, man, intltool, gucharmap, jing, libxml2-devel
BuildRequires: gucharmap-devel, python-devel
Requires(post): desktop-file-utils, shared-mime-info
Requires(postun): desktop-file-utils, shared-mime-info
BuildRoot: %{_tmppath}/%{name}-%{release}-root

Obsoletes: bluefish-shared-data

%description
Bluefish is a powerful editor for experienced web designers and programmers.
Bluefish supports many programming and markup languages, but it focuses on
editing dynamic and interactive websites

%prep
%setup -q -n %{source}

%build
%configure --disable-dependency-tracking \
  --disable-static \
  --disable-update-databases \
  --disable-xml-catalog-update
%{__make} %{?_smp_mflags}

%install
%{__rm} -rf %{buildroot}
make install DESTDIR=%{buildroot}
%{__rm} -f %{buildroot}%{_libdir}/%{name}/*.*a

%find_lang %{name}
%find_lang %{name}_plugin_about
%find_lang %{name}_plugin_charmap
%find_lang %{name}_plugin_entities
%find_lang %{name}_plugin_htmlbar
%find_lang %{name}_plugin_infbrowser
%find_lang %{name}_plugin_snippets
%find_lang %{name}_plugin_zencoding
%{__cat} %{name}_plugin_{about,charmap,entities,htmlbar,infbrowser,snippets,zencoding}.lang >> \
  %{name}.lang

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
%{_datadir}/%{name}/
%{_datadir}/doc/%{name}/*
%{_datadir}/xml/%{name}/*
%{_datadir}/applications/*
%{_datadir}/mime/packages/*
%{_datadir}/pixmaps/*
%{_datadir}/icons/hicolor/*/*/*
%{_libdir}/%{name}/
%{_mandir}/man1/*

%changelog
* Thu Feb  7 2013 Matthias Haase <matthias_haase@bennewitz.com> - 2.2.4-3
- Added python-devel to BuildRequires

* Thu Feb  7 2013 Matthias Haase <matthias_haase@bennewitz.com> - 2.2.4-2
- Update to 2.2.4 Release
- Added libxml2-devel and gucharmap-devel to BuildRequires
- Rebuild for Fedora 18

* Thu Mar  1 2012 Matthias Haase <matthias_haase@bennewitz.com> - 2.2.2-2
- Update to 2.2.2 Release
- Zencoding plugin added

* Thu Nov 24 2011 Matthias Haase <matthias_haase@bennewitz.com> - 2.2.0-2
- Update to 2.2.0 Release
- Rebuild for Fedora 16
- Patches of previous version are removed

* Sun May 29 2011 Matthias Haase <matthias_haase@bennewitz.com> - 2.0.3-4
- Rebuild for Fedora 15
- Patches added
- Jing added to BuildRequires

* Thu Feb 24 2011 Matthias Haase <matthias_haase@bennewitz.com> - 2.0.3-2
- Update to 2.0.3 Release

* Thu Feb 10 2011 Matthias Haase <matthias_haase@bennewitz.com> - 2.0.3-rc2
- Update to 2.0.3 rc2 

* Tue Jan  4 2011 Matthias Haase <matthias_haase@bennewitz.com> - 2.0.3-rc1
- Update to 2.0.3 rc1

* Sat Sep 18 2010 Matthias Haase <matthias_haase@bennewitz.com> - 2.0.2-1
- Update to 2.0.2 Release

* Sat Jun 19 2010 Matthias Haase <matthias_haase@bennewitz.com> - 2.0.1-2
- pcre and pcre-devel removed from Requires and BuildRequires

* Fri Jun 18 2010 Matthias Haase <matthias_haase@bennewitz.com> - 2.0.1
- Update to 2.0.1 Release

* Thu May 27 2010 Matthias Haase <endur@bennew01.localdomain> - 2.0.1-rc1
- Rebuild for Fedora 13
- Remove upstreamed dso-linking and xml-bflang2 patches

* Fri Feb 19 2010 Matthias Haase <matthias_haase@bennewitz.com> - 2.0.0-5
- python-devel removed from BuildRequires

* Thu Feb 18 2010 Matthias Haase <endur@bennew01.localdomain> - 2.0.0-4
- libgnomeui-devel removed from BuildRequires

* Tue Feb 16 2010 Matthias Haase <matthias_haase@bennewitz.com> - 2.0.0-3
- Update to 2.0.0 Release
- dso-linking.patch added (Function log10 in bftextview2.c requires -lm)
- xml-bflang2.patch added
- source url corrected

* Sat Jan 30 2010 Matthias Haase <matthias_haase@bennewitz.com> - 2.0.0-rc3
- Update to 2.0.0-rc3

* Tue Jan 26 2010 Matthias Haase <matthias_haase@bennewitz.com> - 2.0.0-rc2
- Update to 2.0.0-rc2
- enhanced file list for doc and xml directories

* Mon Dec 21 2009 Matthias Haase <matthias_haase@bennewitz.com> - 2.0.0-rc1
- Update to 2.0.0-rc1

* Tue Jun 16 2009 Matthias Haase <matthias_haase@bennewitz.com> - 1.0.7-7
- Rebuild for Fedora 11

* Fri Nov 28 2008 Matthias Haase <matthias_haase@bennewitz.com> - 1.0.7-6
- Rebuild for Fedora 10

* Mon May 19 2008 Matthias Haase <matthias_haase@bennewitz.com> - 1.0.7-5
- Rebuild for Fedora 9

* Tue Nov 13 2007 Matthias Haase <matthias_haase@bennewitz.com> - 1.0.7-4
- Rebuild for Fedora 8

* Sun Jun  3 2007 Matthias Haase <matthias_haase@bennewitz.com> - 1.0.7-3
- Rebuild for Fedora 7
- Category application removed to match desktop entry specification

* Mon Nov  6 2006 Matthias Haase <matthias_haase@bennewitz.com> - 1.0.7-1
- Update to 1.0.7
- mime_icon_assign.patch removed - obsolete 

* Sat Oct 28 2006 Matthias Haase <matthias_haase@bennewitz.com> - 1.0.6-3
- Rebuild for Fedora Core 6

* Mon Oct  2 2006 Matthias Haase <matthias_haase@bennewitz.com> - 1.0.6-2
- Remove of the useless gnome 2.4 mime type registration files
- Patch added for the mime type icon assignment problem

* Tue Sep 26 2006 Matthias Haase <matthias_haase@bennewitz.com> - 1.0.6-1
- Update to 1.0.6 - using latest auto build specfile
- Minor cleanup for %find_lang and %files

* Sat May 06 2006 Matthias Haase <matthias_haase@bennewitz.com>
- Automatic build - snapshot of 2006-05-06
