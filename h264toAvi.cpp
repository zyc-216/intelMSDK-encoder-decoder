#include "addAVIHeader.h"
#include <iostream>

addAVIHeader::addAVIHeader(unsigned width, unsigned height, unsigned bpp, const char* fourcc, unsigned fps, gwavi_audio_t* audio,long lStartIndex)
{

	m_iNowIndex = lStartIndex;
    memset(&avi_header, 0, sizeof(avi_header));
    memset(&stream_header_v, 0, sizeof(stream_header_v));
    memset(&stream_format_v, 0, sizeof(stream_format_v));
    memset(&stream_header_a, 0, sizeof(stream_header_a));
    memset(&stream_format_a, 0, sizeof(stream_format_a));

    marker = 0;
	//markerIndex = 0;
    offsets_ptr = 0;
    offsets_len = 0;
    offsets_start = 0;
    offsets = NULL;
    offset_count = 0;

	/* set avi header */
	avi_header.time_delay = 1000000 / fps;
	avi_header.data_rate = width * height * bpp / 8;
	//avi_header.data_rate = 2000000;
	avi_header.flags = 0x10;

	if (audio)
		avi_header.data_streams = 2;
	else
		avi_header.data_streams = 1;

	/* this field gets updated when calling gwavi_close() */
	avi_header.number_of_frames = 0;
	avi_header.width = width;
	avi_header.height = height;
	avi_header.buffer_size = (width * height * bpp / 8);

	/* set stream header */
	(void)strcpy(stream_header_v.data_type, "vids");
	(void)memcpy(stream_header_v.codec, fourcc, 4);
	stream_header_v.time_scale = 1;
	stream_header_v.data_rate = fps;
	stream_header_v.buffer_size = (width * height * bpp / 8);
	stream_header_v.data_length = 0;

	/* set stream format */
	stream_format_v.header_size = 40;
	stream_format_v.width = width;
	stream_format_v.height = height;
	stream_format_v.num_planes = 1;
	stream_format_v.bits_per_pixel = bpp;
	stream_format_v.compression_type = ((unsigned int)fourcc[3] << 24) + ((unsigned int)fourcc[2] << 16)
		+ ((unsigned int)fourcc[1] << 8) + ((unsigned int)fourcc[0]);
	stream_format_v.image_size = width * height * 3;
	stream_format_v.colors_used = 0;
	stream_format_v.colors_important = 0;

	stream_format_v.palette = NULL;
	stream_format_v.palette_count = 0;

	if (audio) {
		/* set stream header */
		memcpy(stream_header_a.data_type, "auds", 4);
		stream_header_a.codec[0] = 1;
		stream_header_a.codec[1] = 0;
		stream_header_a.codec[2] = 0;
		stream_header_a.codec[3] = 0;
		stream_header_a.time_scale = 1;
		stream_header_a.data_rate = audio->samples_per_second;
		stream_header_a.buffer_size = audio->channels * (audio->bits / 8) * audio->samples_per_second;
		/* when set to -1, drivers use default quality value */
		stream_header_a.audio_quality = -1;
		stream_header_a.sample_size = (audio->bits / 8) * audio->channels;

		/* set stream format */
		stream_format_a.format_type = 1;
		stream_format_a.channels = audio->channels;
		stream_format_a.sample_rate = audio->samples_per_second;
		stream_format_a.bytes_per_second = audio->channels * (audio->bits / 8) * audio->samples_per_second;
		stream_format_a.block_align = audio->channels * (audio->bits / 8);
		stream_format_a.bits_per_sample = audio->bits;
		stream_format_a.size = 0;
	}

	write_chars_bin("RIFF", 4, m_iNowIndex);
	write_int(0, m_iNowIndex);
	write_chars_bin("AVI ", 4, m_iNowIndex);

	write_avi_header_chunk(m_iNowIndex);

	write_chars_bin("LIST", 4, m_iNowIndex);

	//markerIndex = m_dstBuffer.size();
	marker = m_iNowIndex;
	//marker = outFile.tellp();


	write_int(0,m_iNowIndex);
	write_chars_bin("movi", 4,m_iNowIndex);

	offsets_len = 102400;
	offsets = new unsigned int[offsets_len];
	offsets_ptr = 0;

}


addAVIHeader::~addAVIHeader()
{
	//²»ÊÍ·ÅÈÝÆ÷m_dstBufferµÄÄÚ´æ
	delete[] offsets;
}

int addAVIHeader::AddVideoFrameHeader(std::vector<void*>& vecpSrcBuffer, std::vector<int>& veciSrcLength)
{
	int iRet = 0;
	for (int i = 0; i < vecpSrcBuffer.size(); ++i) {
		AddSingleVideoFrameHeader(vecpSrcBuffer[i], veciSrcLength[i], m_iNowIndex);
	}

	Finalize(m_iNowIndex);

	return iRet;
}

int addAVIHeader::Finalize(long lStartIndex)
{
	int ret = 0;
	long t;

	try {
		m_iNowIndex = lStartIndex;
		t = m_iNowIndex;
		//t = outFile.tellp();
		m_iNowIndex = marker;
		//outFile.seekp(marker, ios_base::beg);
		write_int((unsigned int)(t - marker - 4),m_iNowIndex);
		m_iNowIndex = t;
		//outFile.seekp(t, ios_base::beg);

		write_index(offset_count, offsets,m_iNowIndex);

		delete[] offsets;
		offsets = NULL;

		/* reset some avi header fields */
		avi_header.number_of_frames = stream_header_v.data_length;

		t = m_iNowIndex;
		//t = outFile.tellp();
		m_iNowIndex = 12;
		//outFile.seekp(12, ios_base::beg);
		write_avi_header_chunk(m_iNowIndex);
		m_iNowIndex = t;
		//outFile.seekp(t, ios_base::beg);

		t = m_iNowIndex;
		//t = outFile.tellp();
		m_iNowIndex = 4;
		//outFile.seekp(4, ios_base::beg);
		write_int((unsigned int)(t - 8),m_iNowIndex);
		m_iNowIndex = t;
		//outFile.seekp(t, ios_base::beg);

		if (stream_format_v.palette) // TODO check
			delete[] stream_format_v.palette;

		//outFile.close();
	}
	catch (std::system_error& e) {
		std::cerr << e.code().message() << "\n";
		ret = -1;
	}

	return ret;
}

std::vector<unsigned char>& addAVIHeader::GetDstBuffer()
{
	return m_dstBuffer;
}

long addAVIHeader::GetNowIndex()
{
	return m_iNowIndex;
}


void addAVIHeader::write_int(unsigned int n, long lStartIndex)
{
	unsigned char buffer[4];

	buffer[0] = n;
	buffer[1] = n >> 8;
	buffer[2] = n >> 16;
	buffer[3] = n >> 24;

	if (lStartIndex == m_dstBuffer.size()) {
		m_dstBuffer.insert(m_dstBuffer.begin() + lStartIndex, &buffer[0], &buffer[3] + 1);
	}
	else {
		for (int i = 0; i < 4; ++i) {
			m_dstBuffer.at(lStartIndex + i) = buffer[i];
		}
	}
	m_iNowIndex += 4;
}


void addAVIHeader::write_short(unsigned int n, long lStartIndex)
{
	unsigned char buffer[2];

	buffer[0] = n;
	buffer[1] = n >> 8;

	if (lStartIndex == m_dstBuffer.size()) {
		m_dstBuffer.insert(m_dstBuffer.begin() + lStartIndex, &buffer[0], &buffer[1] + 1);
	}
	else{
		for (int i = 0; i < 2; ++i) {
			m_dstBuffer.at(lStartIndex + i) = buffer[i];
		}
	}
	m_iNowIndex += 2;

}



void addAVIHeader::write_chars(const char* s, long lStartIndex)
{
	int count = strlen(s);
	if (count > 255)
		count = 255;
	if (lStartIndex == m_dstBuffer.size()) {
		m_dstBuffer.insert(m_dstBuffer.begin() + lStartIndex, (unsigned char*)&s[0], (unsigned char*)&s[count - 1] + 1);
	}
	else
	{
		for (int i = 0; i < count; ++i) 
		{
			m_dstBuffer.at(lStartIndex + i) = s[i];
		}
	}
	m_iNowIndex += count;
}


void addAVIHeader::write_chars_bin(const char* s, size_t len, long lStartIndex)
{
	//unsigned char ºÍ char?
	if (lStartIndex == m_dstBuffer.size()) {
		m_dstBuffer.insert(m_dstBuffer.begin() + lStartIndex, (unsigned char*)&s[0], (unsigned char*)&s[len - 1] + 1);
	}
	else
	{
		for (int i = 0; i < len; ++i) {
			m_dstBuffer.at(lStartIndex + i) = s[i];
		}
	}
	m_iNowIndex += len;
}


int addAVIHeader::AddSingleVideoFrameHeader(void* pSrcBuffer, int iSrcLength, long lStartIndex)
{
	int ret = 0;
	size_t maxi_pad; /* if your frame is raggin, give it some paddin' */
	size_t t;

	if (!pSrcBuffer)
	{
		fputs("gwavi and/or buffer argument cannot be NULL", stderr);
		return -1;
	}
	if (iSrcLength < 256)
		fprintf(stderr, "WARNING: specified buffer len seems rather small: %d. Are you sure about this?\n", (int)iSrcLength);

	m_iNowIndex = lStartIndex;
	try
	{
		offset_count++;
		stream_header_v.data_length++;

		maxi_pad = iSrcLength % 4;
		if (maxi_pad > 0)
			maxi_pad = 4 - maxi_pad;

		if (offset_count >= offsets_len)
		{
			/*offsets_len += 1024;
			delete[] offsets;
			offsets = new unsigned int[offsets_len];*/
			int tmp_offsets_len = offsets_len;
			offsets_len += 1024;
			unsigned int* tmp_offsets = new unsigned int[offsets_len];
			memcpy(tmp_offsets, offsets, tmp_offsets_len * sizeof(unsigned int));
			delete offsets;
			offsets = tmp_offsets;
		}

		offsets[offsets_ptr++] = (unsigned int)(iSrcLength + maxi_pad);

		write_chars_bin("00dc", 4,m_iNowIndex);

		write_int((unsigned int)(iSrcLength + maxi_pad),m_iNowIndex);

		m_dstBuffer.insert(m_dstBuffer.begin() + m_iNowIndex, (char*)pSrcBuffer, (char*)pSrcBuffer + iSrcLength);
		m_iNowIndex += iSrcLength;
		//outFile.write((char*)buffer, len);
		
		const char* s = "\0";
		for (t = 0; t < maxi_pad; t++)
		{
			m_dstBuffer.insert(m_dstBuffer.begin() + m_iNowIndex, (char*)s, (char*)s + 1);
			m_iNowIndex += 1;
		}
	}
	catch (std::system_error& e)
	{
		std::cerr << e.code().message() << "\n";
		ret = -1;
	}

	return ret;
}

void addAVIHeader::write_avi_header(gwavi_header_t* avi_header, long lStartIndex)
{
	long marker, t;
	m_iNowIndex = lStartIndex;
	write_chars_bin("avih", 4, m_iNowIndex);
	//marker = outFile.tellp();
	marker = m_iNowIndex;
	write_int(0, m_iNowIndex);

	write_int(avi_header->time_delay, m_iNowIndex);
	write_int(avi_header->data_rate, m_iNowIndex);
	write_int(avi_header->reserved, m_iNowIndex);
	/* dwFlags */
	write_int(avi_header->flags, m_iNowIndex);
	/* dwTotalFrames */
	write_int(avi_header->number_of_frames, m_iNowIndex);
	write_int(avi_header->initial_frames, m_iNowIndex);
	write_int(avi_header->data_streams, m_iNowIndex);
	write_int(avi_header->buffer_size, m_iNowIndex);
	write_int(avi_header->width, m_iNowIndex);
	write_int(avi_header->height, m_iNowIndex);
	write_int(avi_header->time_scale, m_iNowIndex);
	write_int(avi_header->playback_data_rate, m_iNowIndex);
	write_int(avi_header->starting_time, m_iNowIndex);
	write_int(avi_header->data_length, m_iNowIndex);

	t = m_iNowIndex;
	//t = outFile.tellp();
	//outFile.seekp(marker, ios_base::beg);
	m_iNowIndex = marker;
	write_int((unsigned int)(t - marker - 4), m_iNowIndex);
	//outFile.seekp(t, ios_base::beg);
	m_iNowIndex = t;
}

void addAVIHeader::write_stream_header(gwavi_stream_header_t* stream_header, long lStartIndex)
{
	long marker, t;
	m_iNowIndex = lStartIndex;
	write_chars_bin("strh", 4, m_iNowIndex);
	marker = m_iNowIndex;
	//marker = outFile.tellp();
	write_int(0, m_iNowIndex);

	write_chars_bin(stream_header->data_type, 4, m_iNowIndex);
	write_chars_bin(stream_header->codec, 4, m_iNowIndex);
	write_int(stream_header->flags, m_iNowIndex);
	write_int(stream_header->priority, m_iNowIndex);
	write_int(stream_header->initial_frames, m_iNowIndex);
	write_int(stream_header->time_scale, m_iNowIndex);
	write_int(stream_header->data_rate, m_iNowIndex);
	write_int(stream_header->start_time, m_iNowIndex);
	write_int(stream_header->data_length, m_iNowIndex);
	write_int(stream_header->buffer_size, m_iNowIndex);
	write_int(stream_header->video_quality, m_iNowIndex);
	write_int(stream_header->sample_size, m_iNowIndex);
	write_int(0, m_iNowIndex);
	write_int(0, m_iNowIndex);

	t = m_iNowIndex;
	//t = outFile.tellp();
	//outFile.seekp(marker, ios_base::beg);
	m_iNowIndex = marker;
	write_int((unsigned int)(t - marker - 4), m_iNowIndex);
	//outFile.seekp(t, ios_base::beg);
	m_iNowIndex = t;
}

void addAVIHeader::write_stream_format_v(gwavi_stream_format_v_t* stream_format_v, long lStartIndex)
{
	long marker, t;
	unsigned int i;
	m_iNowIndex = lStartIndex;
	write_chars_bin("strf", 4, m_iNowIndex);
	marker = m_iNowIndex;
	//marker = outFile.tellp();
	write_int(0, m_iNowIndex);
	write_int(stream_format_v->header_size, m_iNowIndex);
	write_int(stream_format_v->width, m_iNowIndex);
	write_int(stream_format_v->height, m_iNowIndex);
	write_short(stream_format_v->num_planes, m_iNowIndex);
	write_short(stream_format_v->bits_per_pixel, m_iNowIndex);
	write_int(stream_format_v->compression_type, m_iNowIndex);
	write_int(stream_format_v->image_size, m_iNowIndex);
	write_int(stream_format_v->x_pels_per_meter, m_iNowIndex);
	write_int(stream_format_v->y_pels_per_meter, m_iNowIndex);
	write_int(stream_format_v->colors_used, m_iNowIndex);
	write_int(stream_format_v->colors_important, m_iNowIndex);

	if (stream_format_v->colors_used != 0) {
		for (i = 0; i < stream_format_v->colors_used; i++) {
			unsigned char c = stream_format_v->palette[i] & 255;
			m_dstBuffer.insert(m_dstBuffer.begin() + m_iNowIndex, (char*)&c, (char*)&c + 1);
			m_iNowIndex += 1;
			//outFile.write((char*)&c, 1);
			c = (stream_format_v->palette[i] >> 8) & 255;
			m_dstBuffer.insert(m_dstBuffer.begin() + m_iNowIndex, (char*)&c, (char*)&c + 1);
			m_iNowIndex += 1;
			//outFile.write((char*)&c, 1);
			c = (stream_format_v->palette[i] >> 16) & 255;
			m_dstBuffer.insert(m_dstBuffer.begin() + m_iNowIndex, (char*)&c, (char*)&c + 1);
			m_iNowIndex += 1;
			//outFile.write((char*)&c, 1);
			const char* s = "\0";
			m_dstBuffer.insert(m_dstBuffer.begin() + m_iNowIndex, (char*)&s, (char*)&s + 1);
			m_iNowIndex += 1;
			//outFile.write("\0", 1);
		}
	}

	t = m_iNowIndex;
	//t = outFile.tellp();
	m_iNowIndex = marker;
	//outFile.seekp(marker, ios_base::beg);
	write_int((unsigned int)(t - marker - 4), m_iNowIndex);
	m_iNowIndex = t;
	//outFile.seekp(t, ios_base::beg);
}

void addAVIHeader::write_stream_format_a(gwavi_stream_format_a_t* stream_format_a, long lStartIndex)
{
	long marker, t;
	m_iNowIndex = lStartIndex;
	write_chars_bin("strf", 4, m_iNowIndex);
	marker = m_iNowIndex;
	//marker = outFile.tellp();
	write_int(0, m_iNowIndex);
	write_short(stream_format_a->format_type, m_iNowIndex);
	write_short(stream_format_a->channels, m_iNowIndex);
	write_int(stream_format_a->sample_rate, m_iNowIndex);
	write_int(stream_format_a->bytes_per_second, m_iNowIndex);
	write_short(stream_format_a->block_align, m_iNowIndex);
	write_short(stream_format_a->bits_per_sample, m_iNowIndex);
	write_short(stream_format_a->size, m_iNowIndex);

	t = m_iNowIndex;
	//t = outFile.tellp();
	m_iNowIndex = marker;
	//outFile.seekp(marker, ios_base::beg);
	write_int((unsigned int)(t - marker - 4), m_iNowIndex);
	m_iNowIndex = t;
	//outFile.seekp(t, ios_base::beg);
}

void addAVIHeader::write_avi_header_chunk(long lStartIndex)
{
	long marker, t;
	long sub_marker;
	m_iNowIndex = lStartIndex;

	write_chars_bin("LIST", 4, m_iNowIndex);
	marker = m_iNowIndex;
	//marker = outFile.tellp();
	write_int(0, m_iNowIndex);
	write_chars_bin("hdrl", 4, m_iNowIndex);
	write_avi_header(&avi_header, m_iNowIndex);

	write_chars_bin("LIST", 4, m_iNowIndex);
	sub_marker = m_iNowIndex;
	//sub_marker = outFile.tellp();
	write_int(0, m_iNowIndex);
	write_chars_bin("strl", 4, m_iNowIndex);
	write_stream_header(&stream_header_v, m_iNowIndex);
	write_stream_format_v(&stream_format_v, m_iNowIndex);

	t = m_iNowIndex;
	//t = outFile.tellp();

	m_iNowIndex = sub_marker;
	//outFile.seekp(sub_marker, ios_base::beg);
	write_int((unsigned int)(t - sub_marker - 4), m_iNowIndex);
	m_iNowIndex = t;
	//outFile.seekp(t, ios_base::beg);

	if (avi_header.data_streams == 2) {
		write_chars_bin("LIST", 4,m_iNowIndex);
		sub_marker = m_iNowIndex;
		//sub_marker = outFile.tellp();
		write_int(0,m_iNowIndex);
		write_chars_bin("strl", 4, m_iNowIndex);
		write_stream_header(&stream_header_a, m_iNowIndex);
		write_stream_format_a(&stream_format_a, m_iNowIndex);

		t = m_iNowIndex;
		//t = outFile.tellp();
		m_iNowIndex = sub_marker;
		//outFile.seekp(sub_marker, ios_base::beg);
		write_int((unsigned int)(t - sub_marker - 4), m_iNowIndex);
		m_iNowIndex = t;
		//outFile.seekp(t, ios_base::beg);
	}
	t = m_iNowIndex;
	//t = outFile.tellp();
	m_iNowIndex = marker;
	//outFile.seekp(marker, ios_base::beg);
	write_int((unsigned int)(t - marker - 4), m_iNowIndex);
	m_iNowIndex = t;
	//outFile.seekp(t, ios_base::beg);
}



void addAVIHeader::write_index(int count, unsigned int* offsets, long lStartIndex)
{
	long marker, t;
	unsigned int offset = 4;

	if (!offsets)
		throw 1;
	m_iNowIndex = lStartIndex;
	write_chars_bin("idx1", 4, m_iNowIndex);
	marker = m_iNowIndex;
	//marker = outFile.tellp();
	write_int(0, m_iNowIndex);

	for (t = 0; t < count; t++) {
		if ((offsets[t] & 0x80000000) == 0)
			write_chars("00dc", m_iNowIndex);
		else {
			write_chars("01wb", m_iNowIndex);
			offsets[t] &= 0x7fffffff;
		}
		write_int(0x10, m_iNowIndex);
		write_int(offset, m_iNowIndex);
		write_int(offsets[t], m_iNowIndex);

		offset = offset + offsets[t] + 8;
	}

	t = m_iNowIndex;
	//t = outFile.tellp();
	m_iNowIndex = marker;
	//outFile.seekp(marker, ios_base::beg);
	write_int((unsigned int)(t - marker - 4), m_iNowIndex);
	m_iNowIndex = t;
	//outFile.seekp(t, ios_base::beg);
}

int addAVIHeader::check_fourcc(const char* fourcc)
{
	int ret = 0;
	/* list of fourccs from http://fourcc.org/codecs.php */
	const char valid_fourcc[] = "3IV1 3IV2 8BPS"
		"AASC ABYR ADV1 ADVJ AEMI AFLC AFLI AJPG AMPG ANIM AP41 ASLC"
		"ASV1 ASV2 ASVX AUR2 AURA AVC1 AVRN"
		"BA81 BINK BLZ0 BT20 BTCV BW10 BYR1 BYR2"
		"CC12 CDVC CFCC CGDI CHAM CJPG CMYK CPLA CRAM CSCD CTRX CVID"
		"CWLT CXY1 CXY2 CYUV CYUY"
		"D261 D263 DAVC DCL1 DCL2 DCL3 DCL4 DCL5 DIV3 DIV4 DIV5 DIVX"
		"DM4V DMB1 DMB2 DMK2 DSVD DUCK DV25 DV50 DVAN DVCS DVE2 DVH1"
		"DVHD DVSD DVSL DVX1 DVX2 DVX3 DX50 DXGM DXTC DXTN"
		"EKQ0 ELK0 EM2V ES07 ESCP ETV1 ETV2 ETVC"
		"FFV1 FLJP FMP4 FMVC FPS1 FRWA FRWD FVF1"
		"GEOX GJPG GLZW GPEG GWLT"
		"H260 H261 H262 H263 H264 H265 H266 H267 H268 H269"
		"HDYC HFYU HMCR HMRR"
		"I263 ICLB IGOR IJPG ILVC ILVR IPDV IR21 IRAW ISME"
		"IV30 IV31 IV32 IV33 IV34 IV35 IV36 IV37 IV38 IV39 IV40 IV41"
		"IV41 IV43 IV44 IV45 IV46 IV47 IV48 IV49 IV50"
		"JBYR JPEG JPGL"
		"KMVC"
		"L261 L263 LBYR LCMW LCW2 LEAD LGRY LJ11 LJ22 LJ2K LJ44 LJPG"
		"LMP2 LMP4 LSVC LSVM LSVX LZO1"
		"M261 M263 M4CC M4S2 MC12 MCAM MJ2C MJPG MMES MP2A MP2T MP2V"
		"MP42 MP43 MP4A MP4S MP4T MP4V MPEG MPNG MPG4 MPGI MR16 MRCA MRLE"
		"MSVC MSZH"
		"MTX1 MTX2 MTX3 MTX4 MTX5 MTX6 MTX7 MTX8 MTX9"
		"MVI1 MVI2 MWV1"
		"NAVI NDSC NDSM NDSP NDSS NDXC NDXH NDXP NDXS NHVU NTN1 NTN2"
		"NVDS NVHS"
		"NVS0 NVS1 NVS2 NVS3 NVS4 NVS5"
		"NVT0 NVT1 NVT2 NVT3 NVT4 NVT5"
		"PDVC PGVV PHMO PIM1 PIM2 PIMJ PIXL PJPG PVEZ PVMM PVW2"
		"QPEG QPEQ"
		"RGBT RLE RLE4 RLE8 RMP4 RPZA RT21 RV20 RV30 RV40 S422 SAN3"
		"SDCC SEDG SFMC SMP4 SMSC SMSD SMSV SP40 SP44 SP54 SPIG SQZ2"
		"STVA STVB STVC STVX STVY SV10 SVQ1 SVQ3"
		"TLMS TLST TM20 TM2X TMIC TMOT TR20 TSCC TV10 TVJP TVMJ TY0N"
		"TY2C TY2N"
		"UCOD ULTI"
		"V210 V261 V655 VCR1 VCR2 VCR3 VCR4 VCR5 VCR6 VCR7 VCR8 VCR9"
		"VDCT VDOM VDTZ VGPX VIDS VIFP VIVO VIXL VLV1 VP30 VP31 VP40"
		"VP50 VP60 VP61 VP62 VP70 VP80 VQC1 VQC2 VQJC VSSV VUUU VX1K"
		"VX2K VXSP VYU9 VYUY"
		"WBVC WHAM WINX WJPG WMV1 WMV2 WMV3 WMVA WNV1 WVC1"
		"X263 X264 XLV0 XMPG XVID"
		"XWV0 XWV1 XWV2 XWV3 XWV4 XWV5 XWV6 XWV7 XWV8 XWV9"
		"XXAN"
		"Y16 Y411 Y41P Y444 Y8 YC12 YUV8 YUV9 YUVP YUY2 YUYV YV12 YV16"
		"YV92"
		"ZLIB ZMBV ZPEG ZYGO ZYYY";

	if (!fourcc) {
		(void)fputs("fourcc cannot be NULL", stderr);
		return -1;
	}
	if (strchr(fourcc, ' ') || !strstr(valid_fourcc, fourcc))
		ret = 1;

	return ret;
}
