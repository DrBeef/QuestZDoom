#include "sox_i.h"
#include "aac-common.h"
#include <string.h>

// FAAD2
#if defined(HAVE_NEAACDEC_H) || defined(DL_FAAD2)
	#define HAVE_FAAD2
#endif

// FAAC
#if defined(HAVE_FAAC_H) || defined(DL_FAAC)
	#define HAVE_FAAC
#endif

#if defined(HAVE_FAAD2) || defined(HAVE_FAAC)

#ifdef HAVE_FAAD2
	#include <neaacdec.h>
#endif

#ifdef HAVE_FAAC
	#include <faac.h>
#endif

// TODO: Get the symbols later
static const char* const faad2_library_names[] =
{
#ifdef DL_FAAD2
	"faad",
#endif /* DL_FAAD2 */
	NULL
};

typedef struct aac_priv_t {
	unsigned char *aac_buffer;
	size_t aac_buffer_size;

#ifdef HAVE_FAAD2
	// Determines how the file should be decoded
	unsigned long cap; // Capabilities
	NeAACDecHandle hAac; // Decoder handle
	NeAACDecConfigurationPtr conf; // Configuration pointer, maybe modify
	NeAACDecFrameInfo iframe;
#endif /* HAVE_FAAD2 */

} priv_t;

static int startread(sox_format_t * ft)
{
	priv_t *p = (priv_t *) ft->priv;
	
	sox_bool ignore_length = ft->signal.length == SOX_IGNORE_LENGTH;
	int open_library_result = 0;

	// TODO: LSX_DLLIBRARY_OPEN here
	if (open_library_result)
		return SOX_EOF;

	p->aac_buffer_size = sox_globals.bufsiz;
	p->aac_buffer = lsx_malloc(p->aac_buffer_size);

	// TODO: Get the actual file type
	lsx_fail_errno(ft, SOX_EHDR, "Is AAC");
	/*ft->signal.length = SOX_UNSPEC;
	if (ft->seekable) {
		if(!ignore_length)
			ft->signal.length = m4a_duration_ms(ft);
	}*/
	return -1;
}


/*
 * Read up to len samples from p->Synth
 * If needed, read some more AAC data, decode them, and synth them
 * Place in buf[].
 * Return number of samples read.
 */
static size_t sox_aacread(sox_format_t * ft, sox_sample_t * buf, size_t len)
{
	priv_t *p = (priv_t *) ft->priv;

	return -1;
}

static int stopread(sox_format_t *ft)
{

	return -1;
}

static int sox_aacseek(sox_format_t *ft, uint64_t offset)
{

	return -1;
}

static int startwrite(sox_format_t * ft)
{

	return -1;
}

static size_t sox_aacwrite(sox_format_t * ft, const sox_sample_t *buf, size_t samp)
{

	return -1;
}

static int stopwrite(sox_format_t * ft)
{

	return -1;
}

LSX_FORMAT_HANDLER(aac)
{
  static char const * const names[] = {"aac", "audio/aac", NULL};
  static unsigned const write_encodings[] = {
    SOX_ENCODING_AAC, 0, 0};
  static sox_rate_t const write_rates[] = {
    8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 64000, 88200, 96000, 0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "MPEG-4 lossy audio compression", names, 0,
    startread, sox_aacread, stopread,
    startwrite, sox_aacwrite, stopwrite,
    sox_aacseek, write_encodings, write_rates, sizeof(priv_t)
  };
  return &handler;
}
#endif /* defined(HAVE_FAAD2) || defined(HAVE_FAAC) */
