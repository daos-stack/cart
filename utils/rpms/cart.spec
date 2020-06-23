%global carthome %{_exec_prefix}/lib/%{name}

%global mercury_version 2.0.0a1-0.9.git.4871023%{?dist}

Name:          cart
Version:       4.8.1
Release:       1%{?relval}%{?dist}
Summary:       CaRT

License:       Apache
URL:           https://github.com/daos-stack/cart
Source0:       %{name}-%{version}.tar.gz
Source1:       scons_local-%{version}.tar.gz

BuildRequires: scons >= 2.4
BuildRequires: libfabric-devel
BuildRequires: openpa-devel
BuildRequires: mercury-devel = %{mercury_version}
BuildRequires: openmpi3-devel
BuildRequires: libpsm2-devel
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
BuildRequires: Modules
%endif
BuildRequires: gcc-c++
%if %{defined sha1}
Provides: %{name}-%{sha1}
%endif

# This should only be temporary until we can get a stable upstream release
# of mercury, at which time the autoprov shared library version should
# suffice
Requires: mercury = %{mercury_version}

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
Requires: mercury-devel = %{mercury_version}
Requires: openpa-devel
Requires: libfabric-devel
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
sed -i -e 's/\.\.\/etc\//\/etc\//g' %{?buildroot}%{_prefix}/TESTING/{util/cart_utils.py,rpc/cart_rpc_{one,two}_node.yaml}
grep etc %{?buildroot}%{_prefix}/TESTING/{util/cart_utils.py,rpc/cart_rpc_{one,two}_node.yaml}
mv %{?buildroot}%{_prefix}/lib{,64}
mv %{?buildroot}/{usr/,}etc
mkdir -p %{?buildroot}/%{carthome}
cp -al multi-node-test.sh utils %{?buildroot}%{carthome}/
rm -rf %{?buildroot}%{carthome}/utils/{rpms,docker}/
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
%exclude %{_bindir}/crt_launch
%{_libdir}/*.so.*
%dir %{carthome}
%{carthome}/utils
%dir %{_sysconfdir}
%{_sysconfdir}/*
%doc

%files devel
%{_includedir}/*
%{_libdir}/libcart.so
%{_libdir}/libgurt.so

%files tests
%{carthome}/TESTING
%{carthome}/multi-node-test.sh
%{carthome}/.build_vars-Linux.sh
%{_bindir}/crt_launch
%{_bindir}/fault_status


%changelog
* Tue Jun 23 2020 Brian J. Murrell <brian.murrell@intel.com> - 4.8.1-1
- Update mercury version
  - Includes updated license requirements

* Wed May 19 2020 Maureen Jean <maureen.jean@intel.com> - 4.8.0-1
- add fault_status to cart-tests files list

* Tue May 19 2020 Alexander Oganezov <alexander.a.oganezov@intel.com> - 4.7.0-2
- Updated OFI to 8fa7c5bbbfee7df5194b65d9294929a893eb4093
- Added custom sockets_provider.patch to build.config

* Fri May 01 2020 Alexander Oganezov <alexander.a.oganezov@intel.com> - 4.7.0-1
- Bumped version to 4.7.0, as it was previously missed when new fi function
  was added

* Wed Apr 29 2020 Alexander Oganezov <alexander.a.oganezov@intel.com> - 4.6.1-2
- Add memory pinning workaround on server for CART-890.
  New CRT_DISABLE_MEM_PIN envariable added to disable the workaround when set

* Sun Apr 12 2020 Alexander Oganezov <alexander.a.oganezov@intel.com> - 4.6.1-1
- Update version to 4.6.1

* Thu Apr 09 2020 Alexander Oganezov <alexander.a.oganezov@intel.com> - 4.6.0-10
- Update to mercury 4871023 to pick na_ofi.c race condition fix

* Tue Apr 07 2020 Brian J. Murrell <brian.murrell@intel.com> - 4.6.0-9
- Remove openmpi3-devel from cart-devel Requires:

* Mon Apr 06 2020 Brian J. Murrell <brian.murrell@intel.com> - 4.6.0-8
- Clean up excess utils/ content
- Move /usr/etc/ to /etc

* Wed Mar 25 2020 Alexander Oganezov <alexander.a.oganezov@intel.com> - 4.6.0-7
- ofi update 62f6c9

* Tue Mar 17 2020 Alexander Oganezov <alexander.a.oganezov@intel.com> - 4.6.0-6
- mercrury update 41caa1
- ofi update 15ce5c

* Fri Mar 13 2020 Alexander Oganezov <alexander.a.oganezov@intel.com> - 4.6.0-5
- mercury update to mercury_version 2.0.0a1-0.6.git.299b06d

* Wed Mar 11 2020 Alexander Oganezov <alexander.a.oganezov@intel.com> - 4.6.0-4
- mercury update to mercury_version 2.0.0a1-0.5.git.ad5a3b3

* Tue Mar 10 2020 Vikram Chhabra <vikram.chhabra@intel.com> - 4.6.0-3
- mercury_version 2.0.0a1-0.4.git.5d0cd77 - Pulled in HG_Forward fix.

* Mon Mar 09 2020 Brian J. Murrell <brian.murrell@intel.com> - 4.6.0-2
- Move crt_launch to -tests subpackage

* Thu Feb 13 2020 Alexander Oganezov <alexander.a.oganezov@intel.com> - 4.6.0-1
- Libcart version 4.6.0-1
- crt_swim_init()/crt_swim_fini() APIs added
- CRT_FLAG_BIT_AUTO_SWIM_DISABLE flag added to crt_init()

* Tue Feb 11 2020 Alexander Oganezov <alexander.a.oganezov@intel.com> - 4.5.1-2
- Libcart version 4.5.1-2
- mercury_version 2.0.0a1-0.3.git.c2c2628 - unrolled nameserver patch due to
  verbs instability

* Mon Jan 27 2020 Alexander Oganezov <alexander.a.oganezov@intel.com> - 4.5.1-1
- Libcart version 4.5.1-1
- New D_LOG_TRUNCATE environment variable added

* Thu Jan 23 2020 Brian J. Murrell <brian.murrell@intel.com> - 4.5.0-2
- Add Requires: mercury-$version until we can get an upstream stable release

* Sun Jan 19 2020 Alexander Oganezov <alexander.a.oganezov@intel.com> - 4.5.0-1
- Libcart version 4.5.0-1
- crt_barrier() API and associated types removed

* Tue Jan 14 2020 Alexander Oganezov <alexander.a.oganezov@intel.com> - 4.4.0-1
- Libcart version 4.4.0-1
- New IV APIs added: crt_iv_namespace_priv_set/get

* Thu Dec 26 2019 Alexander Oganezov <alexander.a.oganezov@intel.com> - 4.3.1-1
- Libcart version 4.3.1-1
- ofi+verbs provider no longer supported; 'ofi+verbs;ofi_rxm' to be used instead

* Mon Dec 16 2019 Alexander Oganezov <alexander.a.oganezov@intel.com> - 4.3.0-1
- Libcart version 4.3.0-1

* Sat Dec 14 2019 Jeff Olivier <jeffrey.v.olivier@intel.com> - 4.2.0-1
- Libcart version 4.2.0-1
- More modifications to cart build that may affect downstream components

* Wed Dec 11 2019 Jeff Olivier <jeffrey.v.olivier@intel.com> - 4.1.0-1
- Libcart version 4.1.0-1
- OpenMPI build modified to use installed packages
- Add BR: libpsm2-devel since we don't get that with ompi-devel now

* Mon Dec 9 2019 Alexander Oganezov <alexander.a.oganezov@intel.com> - 4.0.0-1
- Libcart version 4.0.0-1
- PMIX support removed along with all associated code

* Thu Dec 5 2019 Alexander Oganezov <alexander.a.oganezov@intel.com> - 3.2.0-3
- Libcart version 3.2.0-3
- Restrict mercury to be version = 1.0.1-21

* Tue Dec 3 2019 Alexander Oganezov <alexander.a.oganezov@intel.com> - 3.2.0-2
- Libcart version 3.2.0-2
- Restrict mercury used to be < 1.0.1-21

* Thu Nov 21 2019 Alexander Oganezov <alexander.a.oganezov@intel.com> - 3.2.0-1
- Libcart version 3.2.0-1
- New DER_GRPVER error code added

* Tue Nov 19 2019 Alexander Oganezov <alexander.a.oganezov@intel.com> - 3.1.0-1
- Libcart version 3.1.0-1
- New crt_group_version_set() API added

* Wed Nov 13 2019 Alexander Oganezov <alexander.a.oganezov@intel.com> - 3.0.0-1
- Libcart version 3.0.0-1
- IV namespace APIs changed

* Mon Nov 11 2019 Brian J. Murrell <brian.murrell@intel.com> - 2.1.0-2
- Don't R: ompi-devel from cart-devel as it breaks the ior
  build which ends up building with ompi instead of mpich
  - the correct solution here is to environment-module-ize
    ompi

* Mon Nov 11 2019 Jeff Olivier <jeffrey.v.olivier@intel.com> - 2.1.0-1
- Libcart version 2.1.0-1
- Add support for registering error codes

* Wed Oct 30 2019 Alexander Oganezov <alexander.a.oganezov@intel.com> - 2.0.0-1
- Libcart version 2.0.0-1
- crt_group_primary_modify, crt_group_secondary_modify APIs changed

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
