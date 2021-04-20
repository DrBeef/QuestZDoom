#include "sox_i.h"

typedef struct {
	size_t buffer_size;
	int16_t * buf;
} priv_t;

extern int nativePush(const int16_t *buf, int nframes);
extern void soxOverDone();
static int startwrite(sox_format_t *ft)
{
	priv_t * p = (priv_t *)ft->priv;
	p->buffer_size = sox_globals.bufsiz;
	p->buf = (int16_t*)lsx_malloc(p->buffer_size * sizeof(int16_t)); // 16 bit = 2 bytes per sample;

	ft->signal.channels = 2;
	ft->signal.rate = 44100;
	ft->encoding.bits_per_sample = 16;
	ft->encoding.encoding = SOX_ENCODING_SIGN2;
	return SOX_SUCCESS;
}

static size_t write_samples(sox_format_t *ft, const sox_sample_t *buf, size_t nsamp)
{
	priv_t * p = (priv_t *)ft->priv;
	SOX_SAMPLE_LOCALS;
	size_t i = nsamp; // Please always be less than bufsiz
	int16_t * buf1 = p->buf;
	while(i--) *buf1++ = SOX_SAMPLE_TO_SIGNED_16BIT(*buf++, ft->clips); // I could do this on the Java side, but this seems more correct
	nativePush((const int16_t*) p->buf, nsamp);
	return nsamp;
}

static int stopwrite(sox_format_t *ft)
{
	free(((priv_t*)ft->priv)->buf);
	soxOverDone();
	return SOX_SUCCESS;
}

static int startread(sox_format_t *ft)
{
	return SOX_EOF;
}

static size_t read_samples(sox_format_t *ft, sox_sample_t *buf, size_t nsamp)
{
	return SOX_EOF;
}
static int stopread(sox_format_t *ft)
{
	return SOX_EOF;
}
LSX_FORMAT_HANDLER(droid)
{
	static char const *const names[] = { "droid", NULL };
	static unsigned const write_encodings[] = {
		SOX_ENCODING_SIGN2, 32, 0, 0
	};
	static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
		"Android AudioTrack client",
		names, SOX_FILE_DEVICE | SOX_FILE_NOSTDIO,
		startread, read_samples, stopread,
		startwrite, write_samples, stopwrite,
		NULL, write_encodings, NULL, sizeof(priv_t)
	};
	return &handler;
}
