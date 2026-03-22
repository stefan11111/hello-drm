XCFLAGS = ${CPPFLAGS} ${CFLAGS} -std=c99 -D_DEFAULT_SOURCE $(shell pkg-config --cflags gbm) $(shell pkg-config --cflags libdrm)

XLDFLAGS = ${LDFLAGS} $(shell pkg-config --libs gbm) $(shell pkg-config --libs libdrm)

MODESETTING_OBJ = \
	modesetting.o \
	main.o

GBM_SCANOUT_OBJ = gbm-scanout.o

ALL_OBJ = ${MODESETTING_OBJ} ${GBM_SCANOUT_OBJ}

all: modesetting gbm-scanout

.c.o:
	${CC} ${XCFLAGS} -c -o $@ $<

modesetting: ${MODESETTING_OBJ}
	${CC} ${XCFLAGS} -o $@ ${MODESETTING_OBJ} ${XLDFLAGS}

gbm-scanout: ${GBM_SCANOUT_OBJ}
	${CC} ${XCFLAGS} -o $@ ${GBM_SCANOUT_OBJ} ${XLDFLAGS}

clean:
	rm -f modesetting gbm-scanout ${ALL_OBJ}
