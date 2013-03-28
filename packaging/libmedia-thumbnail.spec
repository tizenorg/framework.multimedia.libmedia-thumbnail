Name:       libmedia-thumbnail
Summary:    Media thumbnail service library for multimedia applications.
Version: 0.1.69
Release:    1
Group:      utils
License:    Apache
Source0:    %{name}-%{version}.tar.gz

Requires: media-server
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
BuildRequires: pkgconfig(vconf)
BuildRequires: pkgconfig(libmedia-utils)


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
%cmake .
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

#License
mkdir -p %{buildroot}/%{_datadir}/license
cp -rf %{_builddir}/%{name}-%{version}/LICENSE %{buildroot}/%{_datadir}/license/%{name}

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

%files devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/media-thumbnail.pc
%{_includedir}/media-thumbnail/*.h

%files -n media-thumbnail-server
%manifest media-thumbnail-server.manifest
%defattr(-,root,root,-)
%{_bindir}/media-thumbnail-server
/usr/local/bin/test-thumb

