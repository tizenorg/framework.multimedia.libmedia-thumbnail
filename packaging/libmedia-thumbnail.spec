Name:       libmedia-thumbnail
Summary:    Media thumbnail service library for multimedia applications.
Version: 0.1.44
Release:    0
Group:      utils
License:    Apache
Source0:    %{name}-%{version}.tar.gz
BuildRequires: cmake
BuildRequires: pkgconfig(dlog)
BuildRequires: pkgconfig(mm-fileinfo)
BuildRequires: pkgconfig(mmutil-imgp)
BuildRequires: pkgconfig(mmutil-jpeg)
BuildRequires: pkgconfig(drm-client)
BuildRequires: pkgconfig(libexif)
BuildRequires: pkgconfig(heynoti)
BuildRequires: pkgconfig(evas)
BuildRequires: pkgconfig(ecore)
BuildRequires: pkgconfig(aul)


%description
Description: Media thumbnail service library for multimedia applications.


%package devel
License:        Apache
Summary:        Media thumbnail service library for multimedia applications. (development)
Requires:       %{name}  = %{version}-%{release}
Group:          Development/Libraries

%description devel
Description: Media thumbnail service library for multimedia applications. (development)

%package -n media-thumbnail-server
License:        Apache
Summary:        Thumbnail generator.
Requires:       %{name}  = %{version}-%{release}
Group:          Development/Libraries

%description -n media-thumbnail-server
Description: Media Thumbnail Server.


%prep
%setup -q


%build
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}%{_sysconfdir}/rc.d/rc5.d/
ln -s %{_sysconfdir}/init.d/thumbsvr %{buildroot}%{_sysconfdir}/rc.d/rc5.d/S47thumbsvr
mkdir -p %{buildroot}%{_sysconfdir}/rc.d/rc3.d/
ln -s %{_sysconfdir}/init.d/thumbsvr %{buildroot}%{_sysconfdir}/rc.d/rc3.d/S47thumbsvr



%files
%defattr(-,root,root,-)
%{_libdir}/libmedia-thumbnail.so
%{_libdir}/libmedia-thumbnail.so.*
%{_libdir}/libmedia-hash.so
%{_libdir}/libmedia-hash.so.1
%{_libdir}/libmedia-hash.so.1.0.0

%files devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/media-thumbnail.pc
%{_includedir}/media-thumbnail/*.h

%files -n media-thumbnail-server
%defattr(-,root,root,-)
%{_bindir}/media-thumbnail-server
/usr/local/bin/test-thumb
%attr(755,-,-) %{_sysconfdir}/init.d/thumbsvr
%{_sysconfdir}/rc.d/rc3.d/S47thumbsvr
%{_sysconfdir}/rc.d/rc5.d/S47thumbsvr

