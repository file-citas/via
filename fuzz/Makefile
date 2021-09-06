INCLUDE_PATHS = -I$(VIA_LKL_PATH)/tools/lkl/include
INCLUDE_PATHS += -I$(VIA_LKL_PATH)/usr/include/
INCLUDE_PATHS += -Iinclude
INCLUDE_PATHS += -Isrc
CFLAGS = -Wall $(INCLUDE_PATHS)
CFLAGS_DEBUG = $(CFLAGS) -ggdb
LINK_PATHS = -L$(VIA_LKL_PATH)/tools/lkl/lib -Lbin
LDFLAGS = $(LINK_PATHS)
LDFLAGS_FUZZ = $(LINK_PATHS)

TARGET_HARNESS = \
					  bin/harness/harness_net.so \
					  bin/harness/harness_blk_char.so \
					  bin/harness/harness_basic.so

all: bin/libutil.so bin/libfuzz_fw.so bin/fuzzl__X.elf bin/runl__X.elf $(TARGET_HARNESS)

bin/harness/%.o: src/harness/%.c
	$(CC) $(CFLAGS) -g -fsanitize=fuzzer-no-link -fPIC $< -c -o $@
bin/harness/%.so: bin/harness/%.o bin/libutil.so
	$(CC) -shared -lutil $< -o $@

bin/fuzz_fw.o: src/fuzz_fw.c src/fuzz_fw.h
	$(CC) $(CFLAGS) -g -fpic $< -c -o $@
bin/libfuzz_fw.so: bin/fuzz_fw.o
	$(CC) -shared $< -o $@

bin/util.o: src/util.c src/util.h
	$(CC) $(CFLAGS) -fsanitize=fuzzer-no-link -g -fpic $< -c -o $@
bin/libutil.so: bin/util.o
	$(CC) -fsanitize=fuzzer-no-link -shared $< -o $@

bin/runl__%.o: src/fuzz_lib_mod.c
	$(CC) -g -fsanitize=fuzzer-no-link -fsanitize=address -fsanitize-recover=address -DFUZZ_TARGET='"$(lastword $(subst __, ,$(basename $@)))"' $(CFLAGS_DEBUG) -fPIC -c $< -o $@
bin/runl__%.elf: bin/runl__%.o bin/libutil.so
	$(CC) -fsanitize=fuzzer-no-link -fsanitize=address -fsanitize-recover=address $(LDFLAGS_FUZZ) $< -lutil -llkl -lfuzz_fw -lpthread -lm -lrt -ldl -lconfig -o $@

bin/fuzzl__%.o: src/fuzz_lib_mod.c
	$(CC) -g -fsanitize=fuzzer-no-link -fPIC -fsanitize=address -fsanitize-recover=address -DFUZZ_TARGET='"$(lastword $(subst __, ,$(basename $@)))"' -DFUZZ $(CFLAGS_DEBUG) -c $< -o $@
bin/fuzzl__%.elf: bin/fuzzl__%.o bin/libutil.so
	$(CC) $(LDFLAGS_FUZZ) -fsanitize=fuzzer -fsanitize=address -fsanitize-recover=address $< -lutil -llkl -lfuzz_fw -lpthread -lm -lrt -ldl -lconfig -o $@

clean:
	rm -f ./bin/*.o ./bin/*.elf ./bin/*.so ./bin/harness/*.o ./bin/harness/*.so

.PHONY : all clean

