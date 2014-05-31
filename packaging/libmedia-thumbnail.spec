Name:       libmedia-thumbnail
Summary:    Media thumbnail service library for multimedia applications.
Version: 0.1.102
Release:    1
Group:      utils
License:    Apache license v2.0 and public domain
Source0:    %{name}-%{version}.tar.gz

Requires: media-server
BuildRequires: cmake
BuildRequires: pkgconfig(dlog)
BuildRequires: pkgconfig(mm-fileinfo)
BuildRequires: pkgconfig(mmutil-imgp)
BuildRequires: pkgconfig(mmutil-jpeg)
BuildRequires: pkgconfig(drm-client)
BuildRequires: pkgconfig(libexif)
BuildRequires: pkgconfig(evas)
BuildRequires: pkgconfig(ecore)
BuildRequires: pkgconfig(aul)
BuildRequires: pkgconfig(vconf)
BuildRequires: pkgconfig(libmedia-utils)
BuildRequires: pkgconfig(dbus-glib-1)
#exclude tizen_w
%if %{_repository} == "wearable"
BuildRequires: pkgconfig(deviced)
%else
BuildRequires: pkgconfig(pmapi)
%endif


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

%if 0%{?sec_build_binary_debug_enable}
export CFLAGS="$CFLAGS -DTIZEN_DEBUG_ENABLE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_DEBUG_ENABLE"
export FFLAGS="$FFLAGS -DTIZEN_DEBUG_ENABLE"
%endif

cmake \
%if %{_repository} == "wearable"
	-DTIZEN_W=YES \
%endif
	. -DCMAKE_INSTALL_PREFIX=%{_prefix}

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

#License
mkdir -p %{buildroot}/%{_datadir}/license
cp -rf %{_builddir}/%{name}-%{version}/LICENSE %{buildroot}/%{_datadir}/license/%{name}
cp -rf %{_builddir}/%{name}-%{version}/LICENSE %{buildroot}/%{_datadir}/license/media-thumbnail-server

%files
%manifest libmedia-thumbnail.manifest
%defattr(-,root,root,-)
%{_libdir}/libmedia-thumbnail.so
%{_libdir}/libmedia-thumbnail.so.*
%{_libdir}/libmedia-hash.so
%{_libdir}/libmedia-hash.so.1
%{_libdir}/libmedia-hash.so.1.0.0
#License
%{_datadir}/license/%{name}
%{_datadir}/license/media-thumbnail-server

%files devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/media-thumbnail.pc
%{_includedir}/media-thumbnail/*.h

%files -n media-thumbnail-server
%manifest media-thumbnail-server.manifest
%defattr(-,root,root,-)
%{_bindir}/media-thumbnail-server
#/usr/local/bin/test-thumb

