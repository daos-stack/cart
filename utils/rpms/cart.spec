%define carthome %{_exec_prefix}/lib/%{name}

Name:          cart
Version:       1.6.0
Release:       4%{?relval}%{?dist}
Summary:       CaRT

License:       Apache
URL:           https://github.com/daos-stack/cart
Source0:       %{name}-%{version}.tar.gz
Source1:       scons_local-%{version}.tar.gz

BuildRequires: scons >= 2.4
BuildRequires: libfabric-devel
BuildRequires: pmix-devel
BuildRequires: openpa-devel
BuildRequires: mercury-devel >= 1.0.1-19
BuildRequires: ompi-devel
BuildRequires: libevent-devel
BuildRequires: boost-devel
BuildRequires: libuuid-devel
BuildRequires: hwloc-devel
BuildRequires: openssl-devel
BuildRequires: libcmocka-devel
BuildRequires: libyaml-devel
%if (0%{?suse_version} >= 1315)
# these are needed to prefer packages that both provide the same requirement
# prefer over libpsm2-compat
BuildRequires: libpsm_infinipath1
# prefer over libcurl4-mini
BuildRequires: libcurl4
%endif
BuildRequires: gcc-c++
%if %{defined sha1}
Provides: %{name}-%{sha1}
%endif
Requires: mercury < 1.0.1-20

%description
Collective and RPC Transport (CaRT)

CaRT is an open-source RPC transport layer for Big Data and Exascale
HPC. It supports both traditional P2P RPC delivering and collective RPC
which invokes the RPC at a group of target servers with a scalable
tree-based message propagating.

%package devel
Summary: CaRT devel

# since the so is unversioned, it only exists in the main package
# at this time
Requires: %{name} = %{version}-%{release}
Requires: libuuid-devel
Requires: libyaml-devel
Requires: boost-devel
Requires: mercury-devel
Requires: openpa-devel
Requires: libfabric-devel
# can't do this until we can land ompi@PR-10 and
# scons_local@bmurrell/ompi-env-module
#Requires: ompi-devel
Requires: pmix-devel
Requires: hwloc-devel
%if %{defined sha1}
Provides: %{name}-devel-%{sha1}
%endif

%description devel
CaRT devel

%package tests
Summary: CaRT tests

Requires: %{name} = %{version}-%{release}
%if %{defined sha1}
Provides: %{name}-tests-%{sha1}
%endif

%description tests
CaRT tests

%prep
%setup -q -a 1


%build
# remove rpathing from the build
find . -name SConscript | xargs sed -i -e '/AppendUnique(RPATH=.*)/d'

SL_PREFIX=%{_prefix}                      \
scons %{?_smp_mflags}                     \
      --config=force                      \
      USE_INSTALLED=all                   \
      PREFIX=%{?buildroot}%{_prefix}

%install
SL_PREFIX=%{_prefix}                      \
scons %{?_smp_mflags}                     \
      --config=force                      \
      install                             \
      USE_INSTALLED=all                   \
      PREFIX=%{?buildroot}%{_prefix}
BUILDROOT="%{?buildroot}"
PREFIX="%{?_prefix}"
sed -i -e s/${BUILDROOT//\//\\/}[^\"]\*/${PREFIX//\//\\/}/g %{?buildroot}%{_prefix}/TESTING/.build_vars.*
mv %{?buildroot}%{_prefix}/lib{,64}
#mv %{?buildroot}/{usr/,}etc
mkdir -p %{?buildroot}/%{carthome}
cp -al multi-node-test.sh utils %{?buildroot}%{carthome}/
mv %{?buildroot}%{_prefix}/{TESTING,lib/cart/}
ln %{?buildroot}%{carthome}/{TESTING/.build_vars,.build_vars-Linux}.sh

#%if 0%{?suse_version} >= 01315
#%post -n %{suse_libname} -p /sbin/ldconfig
#%postun -n %{suse_libname} -p /sbin/ldconfig
#%else
%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig
#%endif

%files
%defattr(-, root, root, -)
%{_bindir}/*
%{_libdir}/*.so.*
%dir %{carthome}
%{carthome}/utils
%dir %{_prefix}%{_sysconfdir}
%{_prefix}%{_sysconfdir}/*
%doc

%files devel
%{_includedir}/*
%{_libdir}/libcart.so
%{_libdir}/libgurt.so

%files tests
%{carthome}/TESTING
%{carthome}/multi-node-test.sh
%{carthome}/.build_vars-Linux.sh

%changelog
* Mon Nov 11 2019 Brian J. Murrell <brian.murrell@intel.com> - 1.6.0-4
- Don't R: ompi-devel from cart-devel as it breaks the ior
  build whic ends up building with ompi instead of mpich
  - the correct solution here is to environment-module-ize
    ompi

* Thu Nov 06 2019 Brian J. Murrell <brian.murrell@intel.com> - 1.6.0-3
- Pin to mercury < 1.0.1-20 due to incompatibility

* Thu Oct 24 2019 Brian J. Murrell <brian.murrell@intel.com> - 1.6.0-2
- Add BRs to prefer packages that have choices
- Add BR for scons >= 2.4 and gcc-c++
- Add some dirs to %files so they are owned by a package
- Don't unpack the cart tarball twice

* Wed Oct 23 2019 Alexander Oganezov <alexander.a.oganezov@intel.com>
- Libcart version 1.6.0

* Thu Oct 17 2019 Alexander Oganezov <alexander.a.oganezov@intel.com>
- Libcart version 1.5.0

* Wed Oct 16 2019 Alexander Oganezov <alexander.a.oganezov@intel.com>
- Libcart version 1.4.0-2

* Mon Oct 07 2019 Ryon Jensen  <ryon.jensen@intel.com>
- Libcart version 1.4.0

* Wed Sep 25 2019 Dmitry Eremin <dmitry.eremin@intel.com>
- Libcart version 1.3.0

* Mon Sep 23 2019 Jeffrey V. Olivier <jeffrey.v.olivier@intel.com>
- Libcart version 1.2.0

* Thu Aug 08 2019 Alexander A. Oganezov <alexander.a.oganezov@intel.com>
- Libcart version 1.1.0

* Wed Aug 07 2019 Brian J. Murrell <brian.murrell@intel.com>
- Add git hash and commit count to release

* Fri Jul 26 2019 Alexander A. Oganezov <alexander.a.oganezov@intel.com>
- Libcart version 1.0.0

* Fri Jun 21 2019 Brian J. Murrell <brian.murrell@intel.com>
- add Requires: libyaml-devel to the -devel package

* Wed Jun 12 2019 Vikram Chhabra  <vikram.chhabra@intel.com>
- added versioning for libcart and libgurt

* Tue May 07 2019 Brian J. Murrell <brian.murrell@intel.com>
- update for SLES 12.3:
  - libuuid -> libuuid1

* Thu May 02 2019 Brian J. Murrell <brian.murrell@intel.com>
- fix build to use _prefix as install does

* Fri Apr 05 2019 Brian J. Murrell <brian.murrell@intel.com>
- split out devel and tests subpackages
- have devel depend on the main package since we only have the
  unversioned library at the moement which is in the main package

* Wed Apr 03 2019 Brian J. Murrell <brian.murrell@intel.com>
- initial package
