PTHREAD_STATIC_LIB := /usr/lib64/libpthread.a
LIBRT_STATIC_LIB := /usr/lib64/librt.a

all:
	gcc --static -o async_notify.out async_notify.c $(LIBRT_STATIC_LIB) $(PTHREAD_STATIC_LIB)
