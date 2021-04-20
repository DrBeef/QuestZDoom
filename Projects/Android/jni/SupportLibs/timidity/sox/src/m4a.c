#include "sox_i.h"
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

// TODO: Includes faac.h and neaacdec.h
#include "aac-common.h"

#ifdef HAVE_FAAD2
#include <neaacdec.h>
#include <mp4ff.h>
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

typedef struct m4a_priv_t {
	sox_sample_t *m4a_buffer;
	size_t m4a_buffer_size;
	sox_sample_t *m4a_leftover_buf;
	size_t m4a_leftover_count;
	sox_bool seek_pending;
	uint64_t seek_offset;
	sox_bool eof;
#ifdef HAVE_FAAD2
	// Determines how the file should be decoded
	unsigned long cap; // Capabilities
	NeAACDecHandle hAac; // Decoder handle
	NeAACDecConfigurationPtr conf; // Configuration pointer, maybe modify
	NeAACDecFrameInfo iframe;
	mp4AudioSpecificConfig mp4ASC;
	mp4ff_t *infile;
	mp4ff_callback_t *mp4cb;	
	int track; // The track number in the MP4 file that is actually AAC
	unsigned long m4a_samplerate;
	unsigned char m4a_channels;
	//	unsigned int m4a_use_aaclength;
	//unsigned int m4a_initial;
	size_t my_current_sample;
	unsigned int m4a_framesize;
	//	unsigned long m4a_timescale;

#endif /* HAVE_FAAD2 */

} priv_t;
extern uint32_t mp4_read_callback(void *ft_data, void *buffer, uint32_t length);
extern uint32_t mp4_seek_callback(void *ft_data, uint64_t position);

// MP4 specific
uint32_t mp4_read_callback(void *ft_data, void *buffer, uint32_t length)
{
	return lsx_readbuf((sox_format_t *) ft_data, buffer, (size_t) length);
}

uint32_t mp4_seek_callback(void *ft_data, uint64_t position)
{
	// TODO: is seekable?
	// Should return current position?
	int err = lsx_seeki((sox_format_t *) ft_data, (off_t) position, SEEK_SET);
	if (err != SOX_SUCCESS)
	{
		return -1;
	}
	return position;
}

static int mp4_get_aactrack(mp4ff_t *infile)
{
	int i, rc;
	int numTracks = mp4ff_total_tracks(infile);

	for (i = 0; i < numTracks; i++)
	{
		unsigned char *buff = NULL;
		unsigned int buff_size = 0;
		mp4AudioSpecificConfig mp4ASC;

		mp4ff_get_decoder_config(infile, i, &buff, &buff_size);
		if (buff)
		{
			rc = NeAACDecAudioSpecificConfig(buff, buff_size, &mp4ASC);
			free(buff);

			if (rc < 0)
				continue;
			return i;
		}
	}
	return -1;
}

static sox_bool m4a_decode(priv_t *p)
{
	int rc, dur;
	short *sample_buffer;
	unsigned char* buffer;
	unsigned sample = 0;
	unsigned channel = 0;
	sox_sample_t * dst = p->m4a_buffer;
	unsigned int buffer_size;	
	dur = mp4ff_get_sample_duration(p->infile, p->track, p->my_current_sample);
	if( dur <= 0 )
	{
		p->eof = sox_true;
		return sox_false;
	}
	rc = mp4ff_read_sample(p->infile, p->track, p->my_current_sample, &(buffer), &(buffer_size));
	if(rc == 0)
	{
		p->eof = sox_true;
		return sox_false;
	}
	sample_buffer = NeAACDecDecode(p->hAac, &(p->iframe), buffer, buffer_size);
	if(buffer) free(buffer);
	buffer = NULL;
	int sample_count = p->iframe.samples;
	int nsamples = (sample_count)/(p->m4a_channels);
	sox_bool need_leftover = sox_false;
	if(sample_count > p->m4a_buffer_size)
	{
		size_t to_stash = sample_count - p->m4a_buffer_size;
		p->m4a_leftover_buf = lsx_malloc(to_stash*sizeof(sox_sample_t));
		p->m4a_leftover_count = to_stash;
		nsamples = (p->m4a_buffer_size)/(p->m4a_channels);
		p->m4a_buffer += (p->m4a_buffer_size);
		p->m4a_buffer_size = 0;
	}else{
		p->m4a_buffer += sample_count;
		p->m4a_buffer_size -= sample_count;
	}

leftover_copy:
	for(;sample < nsamples; sample++) {
		for(channel = 0; channel < p->m4a_channels; channel++) {
			short s = sample_buffer[channel+sample*p->m4a_channels];
			// TODO only 16 bit for now
			*dst++ = SOX_SIGNED_16BIT_TO_SAMPLE(s,);
		}
	}
	if(sample_count != 0 && sample < p->m4a_framesize ) {
		nsamples = (sample_count)/(p->m4a_channels);
		dst = p->m4a_leftover_buf;
		goto leftover_copy;
	}
	p->my_current_sample++;
	return sox_true;

}

static int startread(sox_format_t * ft)
{
	priv_t *p = (priv_t *) ft->priv;

	sox_bool ignore_length = ft->signal.length == SOX_IGNORE_LENGTH;
	int open_library_result = 0;

	// TODO: LSX_DLLIBRARY_OPEN here
	if (open_library_result)
		return SOX_EOF;

	p->mp4cb = lsx_malloc(sizeof(mp4ff_callback_t));

	p->mp4cb->read = mp4_read_callback;
	p->mp4cb->seek = mp4_seek_callback;
	p->mp4cb->user_data = ft;

	p->hAac = NeAACDecOpen();
	p->conf = NeAACDecGetCurrentConfiguration(p->hAac);
	p->conf->outputFormat = defOutputFormat;
	p->conf->downMatrix = defDownMatrix;

	NeAACDecSetConfiguration(p->hAac, p->conf);

	p->infile = mp4ff_open_read(p->mp4cb);

	if((p->track = mp4_get_aactrack(p->infile)) < 0)
	{
		lsx_fail_errno(ft, SOX_EHDR, "No AAC track in the MP4");
		NeAACDecClose(p->hAac);
		mp4ff_close(p->infile);
		free(p->mp4cb);
		return SOX_EOF;
	}
	p->m4a_buffer = NULL;
	p->m4a_buffer_size = 0;
	mp4ff_get_decoder_config(p->infile, p->track, &(p->m4a_buffer), &(p->m4a_buffer_size));

	if(NeAACDecInit2(p->hAac, p->m4a_buffer, p->m4a_buffer_size, 
				&(p->m4a_samplerate), &(p->m4a_channels)) < 0)
	{
		lsx_fail_errno(ft, SOX_EHDR, "Unable to open FAAD2 decoder");
		NeAACDecClose(p->hAac);
		mp4ff_close(p->infile);
		free(p->mp4cb);
		return SOX_EOF;
	}

	//	p->m4a_timescale = mp4ff_time_scale(p->infile, p->track);
	p->m4a_framesize = 1024;
	//	printf("Timescale: %d", p->m4a_timescale);
	//	p->m4a_use_aaclength = 0;

	if (p->m4a_buffer)
	{
		if (NeAACDecAudioSpecificConfig(p->m4a_buffer, p->m4a_buffer_size, &(p->mp4ASC)))
		{
			if (p->mp4ASC.frameLengthFlag == 1) p->m4a_framesize = 960;
			if (p->mp4ASC.sbr_present_flag == 1) p->m4a_framesize *= 2;
		}
		free(p->m4a_buffer);
	}	

	ft->signal.rate = p->m4a_samplerate;
	ft->encoding.encoding = SOX_ENCODING_M4A;
	ft->signal.precision = 16;
	ft->signal.channels = p->m4a_channels;

	if (ft->seekable) {
		if(!ignore_length) {
			ft->signal.length = (uint64_t)(mp4ff_num_samples(p->infile, p->track)*1024*(p->m4a_channels));
		}
	}

	return (SOX_SUCCESS);
}

/*
 * Decode enough data so that we have up to 'len' samples
 * to place into buf[]
 * 'len' refers to the output samples, not the AAC samples
 */
static size_t sox_m4aread(sox_format_t * ft, sox_sample_t * buf, size_t len)
{
	priv_t *p = (priv_t *) ft->priv;	
	if (p->seek_pending) {
		p->seek_pending = sox_false;
		free(p->m4a_leftover_buf);
		p->m4a_leftover_buf = NULL;
		p->m4a_leftover_count = 0;
		p->m4a_buffer = buf;
		p->m4a_buffer_size = len;
		p->my_current_sample = (p->seek_offset)/(1024*p->m4a_channels);
	}else if(p->m4a_leftover_count > 0) {
		if(len < p->m4a_leftover_count) {
			size_t req_bytes = len * sizeof(sox_sample_t);
			memcpy(buf, p->m4a_leftover_buf, req_bytes);
			p->m4a_leftover_count -= len;
			memmove(p->m4a_leftover_buf, (char*)p->m4a_leftover_buf+req_bytes,
					(size_t)p->m4a_leftover_count * sizeof(sox_sample_t));
			return len;
		}
		memcpy(buf, p->m4a_leftover_buf, p->m4a_leftover_count * sizeof(sox_sample_t));
		p->m4a_buffer = buf + p->m4a_leftover_count;
		p->m4a_buffer_size = len - p->m4a_leftover_count;
		free(p->m4a_leftover_buf);
		p->m4a_leftover_buf = NULL;
		if(p->eof)
		{	
			p->m4a_leftover_count = 0;
			return p->m4a_leftover_count;
		}
		p->m4a_leftover_count = 0;
	}else{
		p->m4a_buffer = buf;
		p->m4a_buffer_size = len;
		if(p->eof)
		{
			return SOX_EOF;
		}
	}
	while(p->m4a_buffer_size && !p->eof) {
		if(!m4a_decode(p))
			break;	
	}

	p->m4a_buffer = NULL;
	return len - p->m4a_buffer_size;
}

static int stopread(sox_format_t *ft)
{
	//	printf("Shutting down\n");
	priv_t *p = (priv_t *) ft->priv;
	NeAACDecClose(p->hAac);
	mp4ff_close(p->infile);
	if(p->mp4cb) free(p->mp4cb);
	if(p->m4a_leftover_buf) free(p->m4a_leftover_buf);
	// Due to weirdness involving mp4ff, 
	// the buffer is repeatedly malloc'd by mp4ff and freed in the same loop
	return SOX_SUCCESS;
}

static int sox_m4aseek(sox_format_t *ft, uint64_t offset)
{
	priv_t *p = (priv_t *) ft->priv;
	p->seek_offset = offset;
	p->seek_pending = sox_true;
	return ft->mode == 'r' ? SOX_SUCCESS : SOX_EOF;
}


// TODO: faac

static int startwrite(sox_format_t * ft)
{

	return SOX_EOF;
}

static size_t sox_m4awrite(sox_format_t * ft, const sox_sample_t *buf, size_t samp)
{

	return SOX_EOF;
}

static int stopwrite(sox_format_t * ft)
{

	return SOX_EOF;
}

LSX_FORMAT_HANDLER(m4a)
{
	static char const * const names[] = {"m4a", "mp4", "audio/mp4", NULL};
	static unsigned const write_encodings[] = {
		SOX_ENCODING_M4A, 0, 0};
	static sox_rate_t const write_rates[] = {
		8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 64000, 88200, 96000, 0};
	static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
		"MPEG-4 lossy audio compression", names, 0,
		startread, sox_m4aread, stopread,
		startwrite, sox_m4awrite, stopwrite,
		sox_m4aseek, write_encodings, write_rates, sizeof(priv_t)
	};
	return &handler;
}
#endif /* defined(HAVE_FAAD2) || defined(HAVE_FAAC) */
