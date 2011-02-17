%define distro	Mandriva Linux
%define name	bluefish
%define version	2.0.2
%define rel		1
%define	release	%mkrel %rel
%define source	%{name}-%{version}

Summary:	A GTK2 web development application for experienced users
Name:		%{name}
Version:		%{version}
Release:		%{release}
URL:		http://bluefish.openoffice.nl
Source:		http://www.bennewitz.com/bluefish/stable/source/%{source}.tar.gz
License:		GPLv2+
Group:          	Development/Web
BuildRoot:	%{_tmppath}/%{source}-buildroot

BuildRequires: glib2-devel, gtk+2-devel >= 2.2.0, gucharmap-devel
BuildRequires: desktop-file-utils, gettext, libxml2, perl-XML-Parser
BuildRequires: enchant-devel, man, intltool
Requires(post): desktop-file-utils, shared-mime-info
Requires(postun): desktop-file-utils, shared-mime-info

Obsoletes: bluefish-shared-data

%description
Bluefish is a powerful editor for experienced web designers and programmers.
Bluefish supports many programming and markup languages, but it focuses on
editing dynamic and interactive websites. This is not a WYSIWYG editor but a
HTML editor (you edit the HTML code).

%prep
%setup -qn %{source}

%build
%configure2_5x --disable-update-databases
%make

%install
rm -rf %{buildroot}
%makeinstall_std

rm -rf %{buildroot}%{_libdir}/%{name}/*.la

%find_lang %{name}
%find_lang %{name}_plugin_about
%find_lang %{name}_plugin_charmap
%find_lang %{name}_plugin_entities
%find_lang %{name}_plugin_htmlbar
%find_lang %{name}_plugin_infbrowser
%find_lang %{name}_plugin_snippets
%{__cat} %{name}_plugin_{about,charmap,entities,htmlbar,infbrowser,snippets}.lang >> \
%{name}.lang

%clean
rm -rf %{buildroot}

%if %mdkversion < 200900
%post
%update_menus
%update_desktop_database
%update_mime_database
%endif
		
%if %mdkversion < 200900
%postun
%clean_menus
%clean_desktop_database
%clean_mime_database
%endif

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
* Thu Sep 16 2010 Wayne Sallee <Wayne@WayneSallee.com> 2.0.2-1mdv2010.1
- rebuild for Mandriva Linux 2010.1 for version 2.0.2
- changed the .spec-file

* Wed Jul 08 2010 Wayne Sallee <Wayne@WayneSallee.com> 2.0.1-1mdv2010.0
- rebuild for Mandriva Linux 2010 for version 2.0.1
- changed the .spec-file

* Thu May 19 2005 Steffen Van Roosbroeck <dansmug@mandrakeclub.nl> 1.0-1.102mcnl
- rebuild for Mandriva Linux LE 2005
- changed the .spec-file

* Wed Jan 12 2005 Matthias Haase <matthias_haase@bennewitz.com>
- Rebuild for version 1.0

* Fri Jan 07 2005 Matthias Haase <matthias_haase@bennewitz.com>
- Automatic build - snapshot of 2005-01-06

* Tue Feb 18 2003 Matthias Haase <matthias_haase@bennewitz.com>
- Rebuild for version 0.9
- Added Makefile patch to support DESTDIR.
- Thanks to Matthias Saou <matthias.saou@est.une.marmotte.net> for t