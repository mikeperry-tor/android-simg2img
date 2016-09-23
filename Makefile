CC      ?= gcc
CXX     ?= g++
LD      ?= gcc
DEP_CC  ?= gcc
AR      ?= ar
RANLIB  ?= ranlib
STRIP   ?= strip
CFLAGS  += -O2 -Wall -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE=1

# libsparse
LIB_NAME = sparse
SLIB     = lib$(LIB_NAME).a
LIB_SRCS = \
    backed_block.c \
    output_file.c \
    sparse.c \
    sparse_crc32.c \
    sparse_err.c \
    sparse_read.c \
    ext4_utils.c \
    ext4_sb.c \
    allocate.c \
    indirect.c \
    extent.c \
    crc16.c \
    sha1.c
LIB_OBJS = $(LIB_SRCS:%.c=%.o)
LIB_INCS = -Iinclude

LDFLAGS += -L. -l$(LIB_NAME) -lm -lz

# simg2img
SIMG2IMG_SRCS = simg2img.c
SIMG2IMG_OBJS = $(SIMG2IMG_SRCS:%.c=%.o)

# simg2simg
SIMG2SIMG_SRCS = simg2simg.c
SIMG2SIMG_OBJS = $(SIMG2SIMG_SRCS:%.c=%.o)

SIMGTRUNC_SRCS = simg_trunc.c

# img2simg
IMG2SIMG_SRCS = $(LIBSPARSE_SRCS) img2simg.c
IMG2SIMG_OBJS = $(IMG2SIMG_SRCS:%.c=%.o)

# append2simg
APPEND2SIMG_SRCS = $(LIBSPARSE_SRCS) append2simg.c
APPEND2SIMG_OBJS = $(APPEND2SIMG_SRCS:%.c=%.o)

# ext2simg
EXT2SIMG_SRCS = ext2simg.c ext4_utils.c
EXT2SIMG_OBJS = $(EXT2SIMG_SRCS:%.c=%.o)

VERITYTREE_SRCS = build_verity_tree.cpp

VERITYKEY_SRCS = generate_verity_key.c

SRCS = \
    $(SIMG2IMG_SRCS) \
    $(SIMG2SIMG_SRCS) \
    $(SIMGTRUNC_SRCS) \
    $(IMG2SIMG_SRCS) \
    $(APPEND2SIMG_SRCS) \
    $(EXT2SIMG_SRCS) \
    $(LIB_SRCS)

.PHONY: default all clean

default: all
all: $(LIB_NAME) simg2img simg2simg img2simg append2simg ext2simg build_verity_tree generate_verity_key simg_trunc

$(LIB_NAME): $(LIB_OBJS)
		$(AR) rc $(SLIB) $(LIB_OBJS)
		$(RANLIB) $(SLIB)

simg2img: $(SIMG2IMG_SRCS)
		$(CC) $(CFLAGS) $(LIB_INCS) -o simg2img $< $(LDFLAGS)

simg2simg: $(SIMG2SIMG_SRCS)
		$(CC) $(CFLAGS) $(LIB_INCS) -o simg2simg $< $(LDFLAGS)

simg_trunc: $(SIMGTRUNC_SRCS)
		$(CC) $(CFLAGS) $(LIB_INCS) -o simg_trunc $< $(LDFLAGS)

img2simg: $(IMG2SIMG_SRCS)$
		$(CC) $(CFLAGS) $(LIB_INCS) -o img2simg $< $(LDFLAGS)

append2simg: $(APPEND2SIMG_SRCS)
		$(CC) $(CFLAGS) $(LIB_INCS) -o append2simg $< $(LDFLAGS)

ext2simg: $(EXT2SIMG_SRCS)
		$(CC) $(CFLAGS) $(LIB_INCS) -o ext2simg $< $(LDFLAGS)

build_verity_tree: $(VERITYTREE_SRCS)
		$(CXX) $(CFLAGS) $(LIB_INCS) -o build_verity_tree $< $(LDFLAGS) -lcrypto

generate_verity_key: $(VERITYKEY_SRCS)
		$(CC) $(CFLAGS) $(LIB_INCS) -o generate_verity_key $< $(LDFLAGS) -lcrypto

%.o: %.c .depend
		$(CC) -c $(CFLAGS) $(LIB_INCS) $< -o $@

clean:
		$(RM) -f *.o *.a simg2img simg2simg img2simg append2simg ext2simg simg_trunc build_verity_tree generate_verity_key.depend

ifneq ($(wildcard .depend),)
include .depend
endif

.depend:
		@$(RM) .depend
		@$(foreach SRC, $(SRCS), $(DEP_CC) $(LIB_INCS) $(SRC) $(CFLAGS) -MT $(SRC:%.c=%.o) -MM >> .depend;)
