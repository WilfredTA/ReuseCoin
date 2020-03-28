TARGET := riscv64-unknown-linux-gnu
CC := $(TARGET)-gcc
LD := $(TARGET)-gcc
OBJCOPY := $(TARGET)-objcopy
CFLAGS := -fPIC -O3 -nostdinc -nostdlib -nostartfiles -fvisibility=hidden -I deps/ckb-c-stdlib -I deps/ckb-c-stdlib/libc -I deps -I deps/molecule -I c -I build -I deps/secp256k1/src -I deps/secp256k1 -Wall -Werror -Wno-nonnull -Wno-nonnull-compare -Wno-unused-function -g
LDFLAGS := -Wl,-static -fdata-sections -ffunction-sections -Wl,--gc-sections
SECP256K1_SRC := deps/secp256k1/src/ecmult_static_pre_context.h
MOLC := moleculec
MOLC_VERSION := 0.5.0
PROTOCOL_HEADER := build/blockchain.h
PROTOCOL_SCHEMA := build/blockchain.mol
PROTOCOL_VERSION := d75e4c56ffa40e17fd2fe477da3f98c5578edcd1
PROTOCOL_DIR := ../../shared/schema/blockchain.mol


# docker pull nervos/ckb-riscv-gnu-toolchain:gnu-bionic-20191012
BUILDER_DOCKER := nervos/ckb-riscv-gnu-toolchain@sha256:aae8a3f79705f67d505d1f1d5ddc694a4fd537ed1c7e9622420a470d59ba2ec3

all: build/sudt build/type_id build/reuse_coin_cell_wallet_lock build/example_reuse

all-via-docker: ${PROTOCOL_HEADER}
	docker run --rm -v `pwd`:/code ${BUILDER_DOCKER} bash -c "cd /code && make"

build/sudt: c/sudt.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
	$(OBJCOPY) --strip-debug --strip-all $@

build/reuse_coin_cell_wallet_lock: c/reuse_coin_cell_wallet_lock.c c/secp256k1_lock.h build/secp256k1_data_info.h $(SECP256K1_SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
	$(OBJCOPY) --strip-debug --strip-all $@

build/type_id: c/type_id.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
	$(OBJCOPY) --strip-debug --strip-all $@

build/example_reuse: c/example_reuse.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
	$(OBJCOPY) --strip-debug --strip-all $@


build/secp256k1_data_info.h: build/dump_secp256k1_data
	$<

build/generate_data_hash: deps/generate_data_hash.c
	gcc -O3 -I deps -o $@ $<

build/dump_secp256k1_data: deps/dump_secp256k1_data.c $(SECP256K1_SRC)
	gcc -O3 -I deps/secp256k1/src -I deps/secp256k1 -o $@ $<


$(SECP256K1_SRC):
	cd deps/secp256k1 && \
		./autogen.sh && \
		CC=$(CC) LD=$(LD) ./configure --with-bignum=no --enable-ecmult-static-precomputation --enable-endomorphism --enable-module-recovery --host=$(TARGET) && \
		make src/ecmult_static_pre_context.h src/ecmult_static_context.h

generate-protocol: check-moleculec-version ${PROTOCOL_HEADER}

check-moleculec-version:
	test "$$(${MOLC} --version | awk '{ print $$2 }' | tr -d ' ')" = ${MOLC_VERSION}

fmt:
	clang-format -i -style=Google $(wildcard c/*.h c/*.c)
	git diff --exit-code $(wildcard c/*.h c/*.c)

${PROTOCOL_HEADER}: ${PROTOCOL_SCHEMA}
	${MOLC} --language c --schema-file $< > $@

${PROTOCOL_SCHEMA}:
	cp ${PROTOCOL_DIR} $@

install-tools:
	if [ ! -x "$$(command -v "${MOLC}")" ] \
			|| [ "$$(${MOLC} --version | awk '{ print $$2 }' | tr -d ' ')" != "${MOLC_VERSION}" ]; then \
		cargo install --force --version "${MOLC_VERSION}" "${MOLC}"; \
	fi

clean:
	rm -rf ${PROTOCOL_HEADER} ${PROTOCOL_SCHEMA}
	rm -rf build/sudt build/type_id build/dump_secp256k1_data build/secp256k1_data build/secp256k1_data_info.h
	rm -rf build/reuse_coin_cell_wallet_lock
	rm -rf build/*.debug
	cd deps/secp256k1 && [ -f "Makefile" ] && make clean

dist: clean all

.PHONY: all all-via-docker dist clean fmt
.PHONY: generate-protocol check-moleculec-version install-tools
