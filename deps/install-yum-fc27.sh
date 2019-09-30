#!/bin/bash

# build-essentials for fedora
yum -y install wget git make automake gcc-c++ openssl-devel kmod-libs \
    kmod-devel libudev-devel json-c-devel libpcap-devel uuid-devel \
    which libuuid libuuid-devel libaio-devel \
    CUnit CUnit-devel librdmacm-devel librdmacm cmake3 numactl-devel python-devel \
    rapidjson-devel gmp-devel mpfr-devel libmpc-devel \
    elfutils-libelf-devel libpcap-devel libuuid-devel libaio-devel boost boost-devel \
    boost-python3 boost-python3-devel gperftools gperftools-devel \
    asciidoc xmlto libtool gtest gtest-devel pkg-config python3 \
    python3-devel gtest-devel \
    libcurl-devel

