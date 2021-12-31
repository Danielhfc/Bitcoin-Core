#!/usr/bin/env bash
#
# Copyright (c) 2019-2021 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

export HOST=x86_64-apple-darwin
export PIP_PACKAGES="zmq lief"
export GOAL="install"
export SYSCOIN_CONFIG="--with-gui --enable-reduce-exports"
export CI_OS_NAME="macos"
export TEST_RUNNER_EXTRA="--exclude interface_zmq_nevm,feature_llmqsimplepose"
export NO_DEPENDS=1
export OSX_SDK=""
export CCACHE_SIZE=300M
export RUN_SECURITY_TESTS="true"
