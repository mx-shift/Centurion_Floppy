#!/usr/bin/bash

set -eu

if [ $# -ne 3 ]; then
  echo "Usage: $(basename $0) <kryo-dir> <hfe-bitrate-kbps> <algorithm>" >&2
  exit -1
fi

SCRIPT_DIR=$(realpath $(dirname $0))

KRYOFLUX_TO_FLASHFLOPPY_DIR=$(realpath ${SCRIPT_DIR}/../kryoflux_to_flashfloppy)
KRYOFLUX_TO_FLASHFLOPPY_BIN=${KRYOFLUX_TO_FLASHFLOPPY_DIR}/target/release/kryoflux_to_flashfloppy

FLASHFLOPPY_TO_HFE_DIR=$(realpath ${SCRIPT_DIR}/../flashfloppy_to_hfe)
FLASHFLOPPY_TO_HFE_BIN=${FLASHFLOPPY_TO_HFE_DIR}/flashfloppy_to_hfe

KRYO=$1
HFE_BITRATE=$2
ALG=$3

BASE=$(basename -s.kryo ${KRYO})

if [ ! -f ${KRYOFLUX_TO_FLASHFLOPPY_BIN} ]; then
  pushd ${KRYOFLUX_TO_FLASHFLOPPY_DIR}
  cargo build --release
  popd
fi

if [ ! -f ${FLASHFLOPPY_TO_HFE_BIN} ]; then
  pushd ${FLASHFLOPPY_TO_HFE_DIR}
  make
  popd
fi

for PRECOMP in 0 300 350 400; do
  FF_SAMPLE_DIR=${BASE}.${PRECOMP}ns.ff_sample
  mkdir -p ${FF_SAMPLE_DIR}
  ${KRYOFLUX_TO_FLASHFLOPPY_BIN} \
    --out-dir ${FF_SAMPLE_DIR} \
    --write-precomp-ns ${PRECOMP} \
    ${KRYO}/${BASE}*00.0.raw

  HFE_DIR=${BASE}.${PRECOMP}ns.${ALG}.hfe
  mkdir -p ${HFE_DIR}
  ${FLASHFLOPPY_TO_HFE_BIN} \
    ${FF_SAMPLE_DIR}/*.revolution1.ff_samples \
    ${HFE_DIR}/${HFE_DIR} \
    ${HFE_BITRATE} \
    ${ALG}
done
