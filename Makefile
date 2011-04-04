

POPPLER_CFLAGS=`pkg-config --cflags poppler`
POPPLER_LIBS=`pkg-config --libs poppler`

CPPFLAGS=-Wall -g ${POPPLER_CFLAGS}
LDFLAGS=${POPPLER_LIBS}

all: pdfclip

clean:
	rm -rf pdfclip
