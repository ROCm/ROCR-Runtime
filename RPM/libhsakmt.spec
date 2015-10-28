%define name        libhsakmt
%define version     1.6.0
%define thunkroot   %{getenv:THUNK_ROOT}

Name:       %{name}
Version:    %{version}
Release:    1
Summary:    Thunk libraries for AMD KFD

Group:      System Environment/Libraries
License:    Advanced Micro Devices Inc.

%description
This package includes the libhsakmt (Thunk) libraries
for AMD KFD

%prep
%setup -T -D -c -n %{name}

%install
mkdir -p $RPM_BUILD_ROOT
cp -R %{thunkroot}/build/rpm/libhsakmt/usr $RPM_BUILD_ROOT

find $RPM_BUILD_ROOT \! -type d | sed "s|$RPM_BUILD_ROOT||"> fw.list

%clean
rm -rf $RPM_BUILD_ROOT

%files -f fw.list
%defattr(-,root,root,-)
