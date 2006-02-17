Name: isp
Version: 0.9
Release: 1
Summary: Industrial Strength Pipes
Group: Application/System
License:  GPL
Url: http://sourceforge.net/projects/isp
BuildRequires: expat-devel, openssl-devel
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root-%(%{__id_u} -n)

%description
Industrial Strength Pipes (ISP) is a toolkit for constructing image 
processing pipelines using the UNIX pipe and filter model.  Like UNIX 
filters, ISP filters can have their standard output and standard input 
chained with a shell command such as:

  foo | bar | baz >result

Unlike UNIX filters which pass unstructured character streams or
newline-separated lines, ISP filters pass structured information in XML
format.  The XML contains a stream of records that are operated on by
filters, analogous to the stream of lines processed grep.

%prep
%setup

%build
make

%install
mkdir -p $RPM_BUILD_ROOT{%{_bindir},%{_libdir},%{_includedir}/isp}
mkdir -p $RPM_BUILD_ROOT{%{_mandir}/man1,%{_mandir}/man3}

cp utils/ispbarrier $RPM_BUILD_ROOT/%{_bindir}
cp utils/ispexec $RPM_BUILD_ROOT/%{_bindir}
cp utils/isprename $RPM_BUILD_ROOT/%{_bindir}
cp utils/ispcat $RPM_BUILD_ROOT/%{_bindir}
cp utils/isprun $RPM_BUILD_ROOT/%{_bindir}
cp utils/ispstats $RPM_BUILD_ROOT/%{_bindir}
cp utils/ispunit $RPM_BUILD_ROOT/%{_bindir}
cp utils/ispunitsplit $RPM_BUILD_ROOT/%{_bindir}
cp utils/ispdelay $RPM_BUILD_ROOT/%{_bindir}
cp utils/ispprogress $RPM_BUILD_ROOT/%{_bindir}
cp utils/ispcount $RPM_BUILD_ROOT/%{_bindir}

cp isp/isp.h $RPM_BUILD_ROOT/%{_includedir}/isp
cp isp/util.h $RPM_BUILD_ROOT/%{_includedir}/isp

cp isp/libisp.so $RPM_BUILD_ROOT/%{_libdir}

cp man/*.1 $RPM_BUILD_ROOT/%{_mandir}/man1
cp man/*.3 $RPM_BUILD_ROOT/%{_mandir}/man3

%clean

%files
%defattr(-,root,root,-)
%doc ChangeLog doc/report.pdf man/manual.pdf DISCLAIMER COPYING
%{_bindir}/*
%{_libdir}/*
%dir %{_includedir}/isp
%{_includedir}/isp/*
%{_mandir}/man1/*
%{_mandir}/man3/*

%changelog
* Tue Dec 06 2005 Jim Garlick <garlick@llnl.gov>
- prep for SF release
