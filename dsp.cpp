#include "stdafx.h"
#include "resource.h"
#include <helpers/DarkMode.h>
#include <phonon.h>

static void RunDSPConfigPopup(const dsp_preset & p_data,HWND p_parent,dsp_preset_edit_callback & p_callback);
IPLContext global_ctx = nullptr;

#define PATH_MAX_SIZE 1024

struct dialogue_settings {
	float input_gain;
	float direct_gain;
	float ref_gain;
	float room_size;
	int cubemat;
	char hrtf_file[(PATH_MAX_SIZE*2)+1];
};

static dialogue_settings default_settings = {
	0.0f,
	0.0f,
	-10.0f,
	2.5f,
	4,
	""
};

static const IPLVector3 cubeverts[] = { 
	{-1.0f,-1.0f,-1.0f},
	{-1.0f,-1.0f, 1.0f},
	{-1.0f, 1.0f, 1.0f},
	{ 1.0f, 1.0f,-1.0f},
	{-1.0f,-1.0f,-1.0f},
	{-1.0f, 1.0f,-1.0f},
	{ 1.0f,-1.0f, 1.0f},
	{-1.0f,-1.0f,-1.0f},
	{ 1.0f,-1.0f,-1.0f},
	{ 1.0f, 1.0f,-1.0f},
	{ 1.0f,-1.0f,-1.0f},
	{-1.0f,-1.0f,-1.0f},
	{-1.0f,-1.0f,-1.0f},
	{-1.0f, 1.0f, 1.0f},
	{-1.0f, 1.0f,-1.0f},
	{ 1.0f,-1.0f, 1.0f},
	{-1.0f,-1.0f, 1.0f},
	{-1.0f,-1.0f,-1.0f},
	{-1.0f, 1.0f, 1.0f},
	{-1.0f,-1.0f, 1.0f},
	{ 1.0f,-1.0f, 1.0f},
	{ 1.0f, 1.0f, 1.0f},
	{ 1.0f,-1.0f,-1.0f},
	{ 1.0f, 1.0f,-1.0f},
	{ 1.0f,-1.0f,-1.0f},
	{ 1.0f, 1.0f, 1.0f},
	{ 1.0f,-1.0f, 1.0f},
	{ 1.0f, 1.0f, 1.0f},
	{ 1.0f, 1.0f,-1.0f},
	{-1.0f, 1.0f,-1.0f},
	{ 1.0f, 1.0f, 1.0f},
	{-1.0f, 1.0f,-1.0f},
	{-1.0f, 1.0f, 1.0f},
	{ 1.0f, 1.0f, 1.0f},
	{-1.0f, 1.0f, 1.0f},
	{ 1.0f,-1.0f, 1.0f}
};

static const IPLTriangle cubetris[] = {
	{0,1,2},
	{3,4,5},
	{6,7,8},
	{9,10,11},
	{12,13,14},
	{15,16,17},
	{18,19,20},
	{21,22,23},
	{24,25,26},
	{27,28,29},
	{30,31,32},
	{33,34,35}
};

static const int nbverts = 36;
static const int nbtris = 12;

static const int nummats = 11; //   0          1         2               3           4         5        6         7         8       9      10
static const wchar_t *mat_names[] = {L"Generic",L"Brick",L"Concrete",L"Ceramic",L"Gravel",L"Carpet",L"Glass",L"Plaster",L"Wood",L"Metal",L"Rock"};
static IPLMaterial materials[] = {  {0.10f,0.20f,0.30f,0.05f,0.100f,0.050f,0.030f},
									{0.03f,0.04f,0.07f,0.05f,0.015f,0.015f,0.015f},
									{0.05f,0.07f,0.08f,0.05f,0.015f,0.002f,0.001f},
									{0.01f,0.02f,0.02f,0.05f,0.060f,0.044f,0.011f},
									{0.60f,0.70f,0.80f,0.05f,0.031f,0.012f,0.008f},
									{0.24f,0.69f,0.73f,0.05f,0.020f,0.005f,0.003f},
									{0.06f,0.03f,0.02f,0.05f,0.060f,0.044f,0.011f},
									{0.12f,0.06f,0.04f,0.05f,0.056f,0.056f,0.004f},
									{0.11f,0.07f,0.06f,0.05f,0.070f,0.014f,0.005f},
									{0.20f,0.07f,0.06f,0.05f,0.200f,0.025f,0.010f},
									{0.13f,0.20f,0.24f,0.05f,0.015f,0.002f,0.001f}};

static inline void make_transform(IPLMatrix4x4 *mat, float x, float y, float z, float sx, float sy, float sz)
{
	mat->elements[0][3] = x;
	mat->elements[1][3] = y;
	mat->elements[2][3] = z;
	mat->elements[0][0] = sx;
	mat->elements[1][1] = sy;
	mat->elements[2][2] = sz;
	mat->elements[3][3] = 1;
}

static inline void apply_matrix(IPLVector3 *vec, IPLMatrix4x4 *mat)
{
	vec->x = vec->x*mat->elements[0][0]+vec->y*mat->elements[0][1]+vec->z*mat->elements[0][2]+mat->elements[0][3];
	vec->y = vec->x*mat->elements[1][0]+vec->y*mat->elements[1][1]+vec->z*mat->elements[1][2]+mat->elements[1][3];
	vec->z = vec->x*mat->elements[2][0]+vec->y*mat->elements[2][1]+vec->z*mat->elements[2][2]+mat->elements[2][3];
}

class dsp_steamaudio : public dsp_impl_base_t<dsp_v3>
{
public:
	dsp_steamaudio(dsp_preset const & in) {
		console::formatter() << "Steam Audio: Hello";
		parse_preset(settings, in);
		if (!setup_steamaudio_ctx())
			console::formatter() << "Steam Audio: Failed setup";
	}

	~dsp_steamaudio() {
		cleanup_buffer();
		cleanup_steamaudio_ctx();
		console::formatter() << "Steam Audio: Goodbye";
	}

	static GUID g_get_guid() {
		// {EA4793E9-3889-412E-8284-84A30FBF9B3B}
		static const GUID guid = { 0xea4793e9, 0x3889, 0x412e, { 0x82, 0x84, 0x84, 0xa3, 0xf, 0xbf, 0x9b, 0x3b } };
		return guid;
	}

	static void g_get_name(pfc::string_base & p_out) { p_out = "Steam Audio DSP";}

	bool on_chunk(audio_chunk * chunk,abort_callback &) {
		// Perform any operations on the chunk here.
		// The most simple DSPs can just alter the chunk in-place here and skip the following functions.
		
		// trivial DSP code: apply our gain to the audio data.
		//chunk->scale( audio_math::gain_to_scale( m_gain ) );

		unsigned chan_cfg = chunk->get_channel_config();
		unsigned samp_rate = chunk->get_sample_rate();
		if (channels_cfg != chan_cfg || sample_rate != samp_rate) {
			unsigned chan_nb = audio_chunk::g_count_channels(chan_cfg);
			channels_cfg = chan_cfg;
			sample_rate = samp_rate;
			channels_nb = chan_nb;
			if (!reconfigure_steamaudio(true)) {
				console::formatter() << "Steam Audio: Failed to reconfigure";
				cleanup_steamaudio();
			}
		}

		//console::formatter() << "Steam Audio chunk size: " << chunk->get_sample_count();

		/*unsigned chan_nb = chunk->get_channels();

		console::formatter() << "Steam Audio buffer: " << audio_buffer_size << "," << audio_buffer_rpos << "," << audio_buffer_wpos;
		console::formatter() << "Steam Audio buffer: " << buffer_get_avail() << "," << buffer_get_free();
		console::formatter() << "Steam Audio chunk size: " << chunk->get_sample_count();
		console::formatter() << "Steam Audio channels nb: " << chan_nb << ", cfg:" << chan_cfg;
		for (int i = 0;i < chunk->get_channels();i++) {
			unsigned chan_flag = audio_chunk::g_extract_channel_flag(chan_cfg, i);
			unsigned long out = 0;
			_BitScanReverse(&out, chan_flag);
			console::formatter() << "Steam Audio channel " << i << ": flag is " << chan_flag << " " << audio_chunk::g_channel_name(chan_flag) << " " << out;
		}*/
		
		buffer_add_data(chunk);

		audio_chunk_impl_temporary pchunk;
		while (buffer_get_chunk(&pchunk)) {
			process_chunk(&pchunk);
			insert_chunk(reinterpret_cast<audio_chunk&>(pchunk));
		}
		pchunk.set_data_size(0);

		// To retrieve the currently processed track, use get_cur_file().
		// Warning: the track is not always known - it's up to the calling component to provide this data and in some situations we'll be working with data that doesn't originate from an audio file.
		// If you rely on get_cur_file(), you should change need_track_change_mark() to return true to get accurate information when advancing between tracks.
 		
		return false; //Return true to keep the chunk or false to drop it from the chain.
	}

	void on_endofplayback(abort_callback &) {
		// The end of playlist has been reached, we've already received the last decoded audio chunk.
		// We need to finish any pending processing and output any buffered data through insert_chunk().
		flush_audio_buffer();
	}

	void on_endoftrack(abort_callback &) {
		// Should do nothing except for special cases where your DSP performs special operations when changing tracks.
		// If this function does anything, you must change need_track_change_mark() to return true.
		// If you have pending audio data that you wish to output, you can use insert_chunk() to do so.
		flush_audio_buffer();
	}

	void flush() {
		// If you have any audio data buffered, you should drop it immediately and reset the DSP to a freshly initialized state.
		// Called after a seek etc.
		audio_buffer_wpos = 0;
		audio_buffer_rpos = 0;
		audio_buffer_empty = true;
	}

	double get_latency() {
		// If the DSP buffers some amount of audio data, it should return the duration of buffered data (in seconds) here.
		return 0;
	}

	bool need_track_change_mark() {
		// Return true if you need on_endoftrack() or need to accurately know which track we're currently processing
		// WARNING: If you return true, the DSP manager will fire on_endofplayback() at DSPs that are before us in the chain on track change to ensure that we get an accurate mark, so use it only when needed.
		return true;
	}

	bool apply_preset(const dsp_preset &preset) {
		// Parse the preset
		dialogue_settings n_set;
		parse_preset(n_set, preset);

		// Figure out what important stuff changed
		bool reconf_room = n_set.room_size != settings.room_size || n_set.cubemat != settings.cubemat;
		bool reconf_hrtf = strcmp(n_set.hrtf_file,settings.hrtf_file) != 0;

		console::formatter() << "Steam Audio: Apply new preset room:" << reconf_room << ", hrtf:" << reconf_hrtf;

		// Apply the change to the current settings
		settings = n_set;

		// If needed reconfigure steamaudio
		return reconfigure_steamaudio(false, reconf_room, reconf_hrtf);
	}

	static bool g_get_default_preset(dsp_preset &p_out) {
		make_preset(default_settings, p_out);
		return true;
	}

	static void g_show_config_popup(const dsp_preset & p_data,HWND p_parent,dsp_preset_edit_callback & p_callback) {
		::RunDSPConfigPopup(p_data, p_parent, p_callback);
	}

	static bool g_have_config_popup() {return true;}

	static void make_preset(dialogue_settings &set, dsp_preset &out) {
		dsp_preset_builder builder;
		builder << set.input_gain << set.direct_gain << set.ref_gain << set.room_size << set.cubemat << set.hrtf_file;
		builder.finish(g_get_guid(), out);
	}

	static void parse_preset(dialogue_settings &set, const dsp_preset &in) {
		try {
			dsp_preset_parser parser(in);
			parser >> set.input_gain >> set.direct_gain >> set.ref_gain >> set.room_size >> set.cubemat >> set.hrtf_file;
		} catch(exception_io_data) {set = default_settings;}
	}

private:

	// Circular buffer impl

	pfc::array_staticsize_t<audio_sample> audio_buffer;
	t_size audio_buffer_size;
	t_size audio_buffer_wpos, audio_buffer_rpos;
	bool audio_buffer_empty;

	inline t_size buffer_get_avail() {
		if (audio_buffer_empty)
			return 0;
		if (audio_buffer_rpos > audio_buffer_wpos)
			return audio_buffer_size-audio_buffer_rpos+audio_buffer_wpos;
		return audio_buffer_wpos-audio_buffer_rpos;
	}

	inline t_size buffer_get_free() {
		if (audio_buffer_empty)
			return audio_buffer_size;
		if (audio_buffer_wpos > audio_buffer_rpos)
			return audio_buffer_size - audio_buffer_wpos + audio_buffer_rpos;
		return audio_buffer_rpos - audio_buffer_wpos;
	}

	void setup_buffer(unsigned chan_nb) {
		console::formatter() << "Steam Audio: Setting up buffer";
		audio_buffer_size = frame_size * 4;
		audio_buffer_wpos = 0;
		audio_buffer_rpos = 0;
		audio_buffer_empty = true;
		audio_buffer.set_size_discard(chan_nb * audio_buffer_size);
	}

	void cleanup_buffer() {
		console::formatter() << "Steam Audio: Cleaning up buffer";
		audio_buffer_size = 0;
		audio_buffer_wpos = 0;
		audio_buffer_rpos = 0;
		audio_buffer_empty = true;
		audio_buffer.set_size_discard(0);
	}

	void buffer_add_data(audio_chunk *chunk) {
		t_size nbsamps = chunk->get_sample_count();
		t_size free = buffer_get_free();
		nbsamps = min(nbsamps, free);
		if (nbsamps > 0)
			audio_buffer_empty = false;
		else
			return;

		audio_sample* data = chunk->get_data();
		audio_sample* buffer = audio_buffer.get_ptr();

		if (audio_buffer_wpos + nbsamps > audio_buffer_size) {
			t_size toput = audio_buffer_size - audio_buffer_wpos;
			memcpy(buffer + audio_buffer_wpos * channels_nb, data, toput * channels_nb * sizeof(audio_sample));
			data += toput * channels_nb;
			nbsamps -= toput;
			audio_buffer_wpos = 0;
		}
		memcpy(buffer + audio_buffer_wpos * channels_nb, data, nbsamps * channels_nb * sizeof(audio_sample));
		audio_buffer_wpos += nbsamps;
	}

	bool buffer_get_chunk(audio_chunk* chunk, bool pad_silence = false) {
		t_size avail = buffer_get_avail();
		if (avail == 0 || (!pad_silence && avail < frame_size))
			return false;
		t_size nbsamps = min(frame_size, avail);

		chunk->set_sample_rate(sample_rate);
		chunk->set_channels(channels_nb, channels_cfg);
		//chunk->set_data_size(frame_size*channels_nb);
		chunk->set_silence(frame_size);

		audio_sample *output = chunk->get_data();
		audio_sample *buffer = audio_buffer.get_ptr();

		if (audio_buffer_rpos > audio_buffer_wpos) {
			t_size toread = min(nbsamps,audio_buffer_size - audio_buffer_rpos);
			memcpy(output, buffer + audio_buffer_rpos * channels_nb, toread * channels_nb * sizeof(audio_sample));
			output += toread * channels_nb;
			nbsamps -= toread;
			audio_buffer_rpos = 0;
		}
		memcpy(output, buffer + audio_buffer_rpos * channels_nb, nbsamps * channels_nb * sizeof(audio_sample));
		audio_buffer_rpos += nbsamps;

		if (audio_buffer_rpos == audio_buffer_wpos)
			audio_buffer_empty = true;

		return true;
	}

	void flush_audio_buffer() {
		audio_chunk_impl_temporary pchunk;
		while (buffer_get_chunk(&pchunk, true)) {
			process_chunk(&pchunk);
			insert_chunk(reinterpret_cast<audio_chunk&>(pchunk));
		}
		pchunk.set_data_size(0);
	}

	// Steam Audio Part

	struct speaker_position {
		float angle;
		float dst;
		float height;
		IPLDirectivity dir;
	};

	static inline float rad(float ang) {
		return ang * (3.14159f / 180.0f);
	}

	const IPLDirectivity dir_omni = {0.0f, 1.0f, nullptr, nullptr};
	const IPLDirectivity dir_ca = {0.5f, 0.5f, nullptr, nullptr};

	//https://www.desmos.com/calculator/m8mprurrou

	/*const speaker_position speaker_positions[18] = {
		{rad(30.0f), 1.0f, 0.0f, dir_ca},	// channel_front_left
		{rad(-30.0f), 1.0f, 0.0f, dir_ca},	// channel_front_right
		{rad(0.0f), 1.0f, 0.0f, dir_ca},	// channel_front_center
		{rad(0.0f), 1.0f, -1.0f, dir_omni},	// channel_lfe
		{rad(140.0f), 1.0f, 0.0f, dir_ca},	// channel_back_left
		{rad(-140.0f), 1.0f, 0.0f, dir_ca},	// channel_back_right
		{rad(10.0f), 1.0f, 0.0f, dir_ca},	// channel_front_center_left
		{rad(-10.0f), 1.0f, 0.0f, dir_ca},	// channel_front_center_right
		{rad(-180.0f), 1.0f, 0.0f, dir_ca},	// channel_back_center
		{rad(90.0f), 1.0f, 0.0f, dir_ca},	// channel_side_left
		{rad(-90.0f), 1.0f, 0.0f, dir_ca},	// channel_side_right
		{rad(0.0f), 0.0f, 1.0f, dir_ca},	// channel_top_center
		{rad(30.0f), 1.0f, 1.0f, dir_ca},	// channel_top_front_left
		{rad(0.0f), 1.0f, 1.0f, dir_ca},	// channel_top_front_center
		{rad(-30.0f), 1.0f, 1.0f, dir_ca},	// channel_top_front_right
		{rad(140.0f), 1.0f, 1.0f, dir_ca},	// channel_top_back_left
		{rad(-180.0f), 1.0f, 1.0f, dir_ca},	// channel_top_back_center
		{rad(-140.0f), 1.0f, 1.0f, dir_ca}	// channel_top_back_right
	};*/

	//https://www.desmos.com/calculator/o8w8s2ploz

	const speaker_position speaker_positions[18] = {
		{rad(-35.0f), 1.0f, 0.0f, dir_ca},	// channel_front_left
		{rad(35.0f), 1.0f, 0.0f, dir_ca},	// channel_front_right
		{rad(0.0f), 1.0f, 0.0f, dir_ca},	// channel_front_center
		{rad(0.0f), 1.0f, -0.5f, dir_omni},	// channel_lfe
		{rad(-140.0f), 1.0f, 0.0f, dir_ca},	// channel_back_left
		{rad(140.0f), 1.0f, 0.0f, dir_ca},	// channel_back_right
		{rad(-10.0f), 1.0f, 0.0f, dir_ca},	// channel_front_center_left
		{rad(10.0f), 1.0f, 0.0f, dir_ca},	// channel_front_center_right
		{rad(180.0f), 1.0f, 0.0f, dir_ca},	// channel_back_center
		{rad(-90.0f), 1.0f, 0.0f, dir_ca},	// channel_side_left
		{rad(90.0f), 1.0f, 0.0f, dir_ca},	// channel_side_right
		{rad(0.0f), 0.0f, 1.0f, dir_ca},	// channel_top_center
		{rad(-35.0f), 1.0f, 1.0f, dir_ca},	// channel_top_front_left
		{rad(0.0f), 1.0f, 1.0f, dir_ca},	// channel_top_front_center
		{rad(35.0f), 1.0f, 1.0f, dir_ca},	// channel_top_front_right
		{rad(-140.0f), 1.0f, 1.0f, dir_ca},	// channel_top_back_left
		{rad(180.0f), 1.0f, 1.0f, dir_ca},	// channel_top_back_center
		{rad(140.0f), 1.0f, 1.0f, dir_ca}	// channel_top_back_right
	};

	struct source_data {
		IPLSource src = nullptr;
		IPLSimulationOutputs sim = {};
		IPLCoordinateSpace3 pos = {};
		IPLVector3 dir = {};
		IPLDirectEffect dir_effect = nullptr;
		IPLBinauralEffect bin_effect = nullptr;
		IPLReflectionEffect ref_effect = nullptr;
	};

	t_size frame_size = 1024*9;
	unsigned channels_cfg = 0;
	unsigned channels_nb = 0;
	unsigned sample_rate = 0;

	IPLAudioSettings audio_cfg = {};
	IPLEmbreeDevice embree_dev = nullptr;
	IPLScene scene = nullptr;
	IPLSimulationSettings simulation_settings = {};
	IPLSimulationFlags sim_flags;
	IPLStaticMesh static_mesh = nullptr;
	IPLSimulator simulator = nullptr;
	IPLHRTF hrtf = nullptr;
	IPLAmbisonicsDecodeEffect ambi_effect = nullptr;
	IPLReflectionMixer ref_mixer = nullptr;
	IPLAudioBuffer buffer_input = {};
	IPLAudioBuffer buffer_mono = {};
	IPLAudioBuffer buffer_stereo1 = {};
	IPLAudioBuffer buffer_stereo2 = {};
	IPLAudioBuffer buffer_ambi = {};
	pfc::array_staticsize_t<source_data> sources;
	IPLSimulationInputs source_inputs = {};
	IPLCoordinateSpace3 listener_coords = {};
	IPLBinauralEffectParams bin_params = {};
	IPLDirectEffectFlags dir_apply_flags;
	IPLReflectionEffectSettings ref_settings = {};

	static inline IPLVector3 cross_prod(IPLVector3 a, IPLVector3 b) {
		return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
	}

	static inline float length(IPLVector3 vect) {
		return sqrtf(vect.x*vect.x + vect.y*vect.y + vect.z*vect.z);
	}

	static inline IPLVector3 scale(IPLVector3 vect, float val) {
		return {vect.x*val, vect.y*val, vect.z*val};
	}

	static inline IPLVector3 normalize(IPLVector3 vect) {
		return scale(vect, 1.0f/length(vect));
	}

	static inline void set_coord(IPLCoordinateSpace3 *coord, IPLVector3 origin, IPLVector3 dir) {
		coord->origin = origin;
		coord->ahead = dir;
		IPLVector3 up = {0.0f, 1.0f, 0.0f};
		coord->right = normalize(cross_prod(dir, up));
		coord->up = normalize(cross_prod(coord->right, dir));
	}

	static inline void make_pos(IPLCoordinateSpace3 *coord, float angle, float dst, float height) {
		IPLVector3 origin = {sinf(angle)*dst, height, -cosf(angle)*dst};
		IPLVector3 dir = normalize(scale(origin,-1.0f));
		set_coord(coord, origin, dir);
	}

	static inline void make_pos(IPLCoordinateSpace3 *coord, speaker_position pos) {
		make_pos(coord, pos.angle, pos.dst, pos.height);
	}

	static inline unsigned get_channel_idx(unsigned flag) {
		switch (flag) {
			case audio_chunk::channel_front_left: return 0;
			case audio_chunk::channel_front_right: return 1;
			case audio_chunk::channel_front_center: return 2;
			case audio_chunk::channel_lfe: return 3;
			case audio_chunk::channel_back_left: return 4;
			case audio_chunk::channel_back_right: return 5;
			case audio_chunk::channel_front_center_left: return 6;
			case audio_chunk::channel_front_center_right: return 7;
			case audio_chunk::channel_back_center: return 8;
			case audio_chunk::channel_side_left: return 9;
			case audio_chunk::channel_side_right: return 10;
			case audio_chunk::channel_top_center: return 11;
			case audio_chunk::channel_top_front_left: return 12;
			case audio_chunk::channel_top_front_center: return 13;
			case audio_chunk::channel_top_front_right: return 14;
			case audio_chunk::channel_top_back_left: return 15;
			case audio_chunk::channel_top_back_center: return 16;
			case audio_chunk::channel_top_back_right: return 17;
			default: return 2;
		}
	}

	static inline void clear_buffer(IPLAudioBuffer *buf) {
		for (int c=0;c<buf->numChannels;c++)
			memset(buf->data[c],0,buf->numSamples*sizeof(float));
	}

	static inline void scale_buffer(IPLAudioBuffer *buf, float val) {
		for (int c=0;c<buf->numChannels;c++) {
			float *dat = buf->data[c];
			for (int i=0;i<buf->numSamples;i++)
				dat[i] *= val;
		}
	}

	// SA Processing on audio chunk
	void process_chunk(audio_chunk *chunk) {
		if (sources.get_size() != channels_nb)
			return;

		if (settings.input_gain != 0.0f)
			chunk->scale((audio_sample)audio_math::gain_to_scale(settings.input_gain));

		iplAudioBufferDeinterleave(global_ctx, chunk->get_data(), &buffer_input);

		float *input_ptr = nullptr;

		IPLAudioBuffer input_buffer = {};
		input_buffer.numChannels = 1;
		input_buffer.numSamples = frame_size;
		input_buffer.data = &input_ptr;

		clear_buffer(&buffer_stereo2);
		IPLReflectionEffectParams refParams;
		//refParams.numChannels = buffer_ambi.numChannels;
		//refParams.irSize = static_cast<IPLint32>(simulation_settings.maxDuration*sample_rate);

		IPLAmbisonicsDecodeEffectParams ambiParams = {};
		ambiParams.order = simulation_settings.maxOrder;
		ambiParams.hrtf = hrtf;
		ambiParams.orientation = listener_coords;
		ambiParams.binaural = IPL_TRUE;

		for (unsigned i=0;i<channels_nb;i++) {
			source_data *src = &sources[i];
			bin_params.direction = src->dir;
			IPLDirectEffectParams dirParams = src->sim.direct;

			input_ptr = buffer_input.data[i];

			iplDirectEffectApply(src->dir_effect, &dirParams, &input_buffer, &buffer_mono);

			iplBinauralEffectApply(src->bin_effect, &bin_params, &buffer_mono, &buffer_stereo1);

			if (settings.direct_gain != 0.0f)
				scale_buffer(&buffer_stereo1, (audio_sample)audio_math::gain_to_scale(settings.direct_gain));

			iplAudioBufferMix(global_ctx, &buffer_stereo1, &buffer_stereo2);

			refParams = src->sim.reflections;

			iplReflectionEffectApply(src->ref_effect, &refParams, &input_buffer, &buffer_ambi, ref_mixer);
		}

		iplReflectionMixerApply(ref_mixer, &refParams, &buffer_ambi);

		iplAmbisonicsDecodeEffectApply(ambi_effect, &ambiParams, &buffer_ambi, &buffer_stereo1);

		if (settings.ref_gain != 0.0f)
			scale_buffer(&buffer_stereo1, (audio_sample)audio_math::gain_to_scale(settings.ref_gain));

		iplAudioBufferMix(global_ctx, &buffer_stereo1, &buffer_stereo2);

		chunk->set_channels(2, audio_chunk::channel_config_stereo);
		chunk->set_data_size(frame_size*2);

		iplAudioBufferInterleave(global_ctx, &buffer_stereo2, chunk->get_data());
	}

	void run_simulation() {
		console::formatter() << "Steam Audio: Simulating";

		for (unsigned i=0;i<channels_nb;i++) {
			source_data *srcd = &sources[i];
			IPLSource src = srcd->src;
			unsigned flag = audio_chunk::g_extract_channel_flag(channels_cfg, i);
			speaker_position pos = speaker_positions[get_channel_idx(flag)];
			make_pos(&srcd->pos, pos);
			source_inputs.source = srcd->pos;
			source_inputs.directivity = pos.dir;
			iplSourceSetInputs(src, sim_flags, &source_inputs);
			srcd->dir = iplCalculateRelativeDirection(global_ctx,srcd->pos.origin,listener_coords.origin,
						listener_coords.ahead,listener_coords.up);
		}

		iplSimulatorRunDirect(simulator);
		iplSimulatorRunReflections(simulator);

		for (unsigned i=0;i<channels_nb;i++) {
			IPLSource src = sources[i].src;
			iplSourceGetOutputs(src, sim_flags, &sources[i].sim);
			sources[i].sim.direct.flags = dir_apply_flags;
		}
	}

	bool setup_sources() {
		IPLDirectEffectSettings dir_settings = {};
		dir_settings.numChannels = 1;

		IPLBinauralEffectSettings bin_settings = { hrtf };

		sources.set_size_discard(channels_nb);
		for (unsigned i=0;i<channels_nb;i++) {
			source_data *src = &sources[i];
			if (iplDirectEffectCreate(global_ctx, &audio_cfg, &dir_settings, &src->dir_effect) != IPL_STATUS_SUCCESS)
				return false;

			if (iplBinauralEffectCreate(global_ctx, &audio_cfg, &bin_settings, &src->bin_effect) != IPL_STATUS_SUCCESS)
				return false;

			if (iplReflectionEffectCreate(global_ctx, &audio_cfg, &ref_settings, &src->ref_effect) != IPL_STATUS_SUCCESS)
				return false;

			IPLSourceSettings source_settings = {};
			source_settings.flags = sim_flags;

			if (iplSourceCreate(simulator, &source_settings, &src->src) != IPL_STATUS_SUCCESS)
				return false;
			
			iplSourceAdd(src->src, simulator);
		}
		return true;
	}

	void cleanup_sources() {
		for (unsigned i=0;i<sources.get_size();i++) {
			source_data *src = &sources[i];
			if (src->src) {
				iplSourceRemove(src->src, simulator);
				iplSimulatorCommit(simulator);
				iplSourceRelease(&src->src);
			}
			if (src->dir_effect)
				iplDirectEffectRelease(&src->dir_effect);
			if (src->bin_effect)
				iplBinauralEffectRelease(&src->bin_effect);
			if (src->ref_effect)
				iplReflectionEffectRelease(&src->ref_effect);
		}
		sources.set_size_discard(0);
	}

	// Where the actual SA Setup occurs
	bool reconfigure_steamaudio(bool reconf_audio_cfg=true, bool reconf_room=false, bool reconf_hrtf=false) {
		if (!reconf_audio_cfg && !reconf_room && !reconf_hrtf)
			return true;
		console::formatter() << "Steam Audio: Reconfiguring Audio";

		IPLSceneType sceneType = IPL_SCENETYPE_EMBREE;

		if (reconf_audio_cfg) {
			console::formatter() << "Steam Audio: Full Reconf";
			reconf_hrtf = false;
			reconf_room = false;

			cleanup_steamaudio();
			setup_buffer(channels_nb);
			audio_cfg.frameSize = frame_size;
			audio_cfg.samplingRate = sample_rate;

			embree_dev = nullptr;

			if (sceneType == IPL_SCENETYPE_EMBREE) {
				IPLEmbreeDeviceSettings embreeSettings = {};
				if (iplEmbreeDeviceCreate(global_ctx, &embreeSettings, &embree_dev) != IPL_STATUS_SUCCESS)
					return false;
			}

			IPLSceneSettings sceneSettings = {};
			sceneSettings.type = sceneType;
			sceneSettings.embreeDevice = embree_dev;

			if (iplSceneCreate(global_ctx, &sceneSettings, &scene) != IPL_STATUS_SUCCESS)
				return false;
		}

		if (reconf_room && static_mesh) {
			iplStaticMeshRemove(static_mesh, scene);
			iplSceneCommit(scene);
			iplStaticMeshRelease(&static_mesh);
		}

		if (reconf_audio_cfg || reconf_room) {
			const int imeshsnb = 6;
			float scale = settings.room_size;
			float fagarr[][6] = {
				{0,-2.15,0,2*scale,1,2*scale},
				{0,2.15,0,2*scale,1,2*scale},
				{1*scale,0,0,1,2*scale,2*scale},
				{-1*scale,0,0,1,2*scale,2*scale},
				{0,0,1*scale,2*scale,2*scale,1},
				{0,0,-1*scale,2*scale,2*scale,1}
			};
			//IPLhandle imeshs[imeshsnb];
			IPLVector3 verts[imeshsnb*nbverts];
			IPLTriangle tris[imeshsnb*nbtris];
			IPLint32 mats[imeshsnb*nbtris];
			for (int i=0;i<imeshsnb;i++) {
				IPLMatrix4x4 mat = {};
				make_transform(&mat,fagarr[i][0],fagarr[i][1],fagarr[i][2],fagarr[i][3],fagarr[i][4],fagarr[i][5]);
				for (int i2=0;i2<nbverts;i2++) {
					verts[(i*nbverts)+i2].x = cubeverts[i2].x;
					verts[(i*nbverts)+i2].y = cubeverts[i2].y;
					verts[(i*nbverts)+i2].z = cubeverts[i2].z;
					apply_matrix(&verts[(i*nbverts)+i2],&mat);
				}
				for (int i2=0;i2<nbtris;i2++) {
					tris[(i*nbtris)+i2].indices[0] = (i*nbverts)+cubetris[i2].indices[0];
					tris[(i*nbtris)+i2].indices[1] = (i*nbverts)+cubetris[i2].indices[1];
					tris[(i*nbtris)+i2].indices[2] = (i*nbverts)+cubetris[i2].indices[2];
					mats[(i*nbtris)+i2] = settings.cubemat;
				}
				//iplCreateInstancedMesh(scene, cube_scene, mat, &imeshs[i]);
				//iplAddInstancedMesh(scene, imeshs[i]);
			}

			//printf("Verts:%i Tris:%i\n",sizeof(verts)/sizeof(IPLVector3),sizeof(tris)/sizeof(IPLTriangle));

			IPLStaticMeshSettings staticMeshSettings = {};
			staticMeshSettings.numVertices = nbverts * imeshsnb;
			staticMeshSettings.numTriangles = nbtris * imeshsnb;
			staticMeshSettings.numMaterials = nummats;
			staticMeshSettings.vertices = verts;
			staticMeshSettings.triangles = tris;
			staticMeshSettings.materialIndices = mats;
			staticMeshSettings.materials = materials;

			if (iplStaticMeshCreate(scene, &staticMeshSettings, &static_mesh) != IPL_STATUS_SUCCESS)
				return false;

			iplStaticMeshAdd(static_mesh, scene);
			iplSceneCommit(scene);
			//iplSceneSaveOBJ(scene,"C:\\Users\\Pablo\\AppData\\Roaming\\foobar2000\\user-components\\foo_steamaudio\\fuck.obj");
		}

		int nbRays = 4096;
		int nbBounces = 16;
		int nbOccSamples = 128;
		int ambiOrder = 3;
		int ambiChans = (ambiOrder + 1) * (ambiOrder + 1);
		float irDuration = 2.0f;

		if (reconf_audio_cfg) {
			sim_flags = static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_DIRECT | IPL_SIMULATIONFLAGS_REFLECTIONS);

			simulation_settings.flags = sim_flags;
			simulation_settings.sceneType = sceneType;
			simulation_settings.reflectionType = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION;
			simulation_settings.maxNumOcclusionSamples = nbOccSamples;
			simulation_settings.maxNumRays = nbRays;
			simulation_settings.numDiffuseSamples = 32;
			simulation_settings.maxDuration = irDuration;
			simulation_settings.maxOrder = ambiOrder;
			simulation_settings.maxNumSources = audio_chunk::defined_channel_count;
			simulation_settings.numThreads = 24;
			simulation_settings.samplingRate = sample_rate;
			simulation_settings.frameSize = frame_size;

			if (iplSimulatorCreate(global_ctx, &simulation_settings, &simulator) != IPL_STATUS_SUCCESS)
				return false;

			iplSimulatorSetScene(simulator, scene);
			iplSimulatorCommit(simulator);
		}

		if (reconf_hrtf) {
			cleanup_sources();
			if (ambi_effect)
				iplAmbisonicsDecodeEffectRelease(&ambi_effect);
			if (hrtf)
				iplHRTFRelease(&hrtf);
		}

		if (reconf_audio_cfg || reconf_hrtf) {
			IPLHRTFSettings hrtf_params = { IPL_HRTFTYPE_DEFAULT, NULL, NULL, 0, 1.0f };

			if (settings.hrtf_file[0] != '\x00') {
				hrtf_params.type = IPL_HRTFTYPE_SOFA;
				hrtf_params.sofaFileName = (const char*)&settings.hrtf_file;
			}

			if (iplHRTFCreate(global_ctx, &audio_cfg, &hrtf_params, &hrtf) != IPL_STATUS_SUCCESS)
				return false;
		}

		ref_settings.type = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION;
		ref_settings.irSize = static_cast<IPLint32>(simulation_settings.maxDuration * sample_rate);
		ref_settings.numChannels = ambiChans;

		if (reconf_audio_cfg || reconf_hrtf) {
			if (!setup_sources())
				return false;
		}

		if (reconf_audio_cfg || reconf_hrtf) {
			IPLAmbisonicsDecodeEffectSettings ambi_settings = {};
			ambi_settings.speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_STEREO;
			ambi_settings.maxOrder = ambiOrder;
			ambi_settings.hrtf = hrtf;

			if (iplAmbisonicsDecodeEffectCreate(global_ctx, &audio_cfg, &ambi_settings, &ambi_effect) != IPL_STATUS_SUCCESS)
				return false;
		}

		if (reconf_audio_cfg) {
			if (iplReflectionMixerCreate(global_ctx, &audio_cfg, &ref_settings, &ref_mixer) != IPL_STATUS_SUCCESS)
				return false;

			if (iplAudioBufferAllocate(global_ctx, channels_nb, frame_size, &buffer_input) != IPL_STATUS_SUCCESS)
				return false;
			if (iplAudioBufferAllocate(global_ctx, 1, frame_size, &buffer_mono) != IPL_STATUS_SUCCESS)
				return false;
			if (iplAudioBufferAllocate(global_ctx, 2, frame_size, &buffer_stereo1) != IPL_STATUS_SUCCESS)
				return false;
			if (iplAudioBufferAllocate(global_ctx, 2, frame_size, &buffer_stereo2) != IPL_STATUS_SUCCESS)
				return false;
			if (iplAudioBufferAllocate(global_ctx, ambiChans, frame_size, &buffer_ambi) != IPL_STATUS_SUCCESS)
				return false;
		}

		if (reconf_audio_cfg || reconf_hrtf) {
			listener_coords.origin = {0.0f, 0.0f, 0.0f};
			listener_coords.up = {0.0f, 1.0f, 0.0f};
			listener_coords.right = {1.0f, 0.0f, 0.0f};
			listener_coords.ahead = {0.0f, 0.0f, -1.0f};

			IPLSimulationSharedInputs sharedInputs = {};
			sharedInputs.listener = listener_coords;
			sharedInputs.numRays = nbRays;
			sharedInputs.numBounces = nbBounces;
			sharedInputs.duration = irDuration;
			sharedInputs.order = ambiOrder;
			sharedInputs.irradianceMinDistance = 1.0f;
			iplSimulatorSetSharedInputs(simulator, sim_flags, &sharedInputs);

			iplSimulatorCommit(simulator);

			source_inputs.flags = sim_flags;
			source_inputs.directFlags = static_cast<IPLDirectSimulationFlags>(IPL_DIRECTSIMULATIONFLAGS_DISTANCEATTENUATION |
										IPL_DIRECTSIMULATIONFLAGS_AIRABSORPTION |
										IPL_DIRECTSIMULATIONFLAGS_DIRECTIVITY |
										IPL_DIRECTSIMULATIONFLAGS_OCCLUSION |
										IPL_DIRECTSIMULATIONFLAGS_TRANSMISSION);
			source_inputs.distanceAttenuationModel.type = IPL_DISTANCEATTENUATIONTYPE_DEFAULT;
			source_inputs.airAbsorptionModel.type = IPL_AIRABSORPTIONTYPE_DEFAULT;
			source_inputs.occlusionType = IPL_OCCLUSIONTYPE_VOLUMETRIC;
			source_inputs.occlusionRadius = 1.0f;
			source_inputs.numOcclusionSamples = nbOccSamples;
		}

		if (reconf_audio_cfg || reconf_room || reconf_hrtf)
			run_simulation();

		if (reconf_audio_cfg || reconf_hrtf) {
			bin_params.interpolation = IPL_HRTFINTERPOLATION_BILINEAR;
			bin_params.spatialBlend = 1.0f;
			bin_params.hrtf = hrtf;

			dir_apply_flags = static_cast<IPLDirectEffectFlags>(IPL_DIRECTEFFECTFLAGS_APPLYDISTANCEATTENUATION |
								IPL_DIRECTEFFECTFLAGS_APPLYAIRABSORPTION |
								IPL_DIRECTEFFECTFLAGS_APPLYDIRECTIVITY |
								IPL_DIRECTEFFECTFLAGS_APPLYOCCLUSION |
								IPL_DIRECTEFFECTFLAGS_APPLYTRANSMISSION);
		}

		return true;
	}

	void cleanup_steamaudio() {
		cleanup_sources();
		if (buffer_ambi.data)
			iplAudioBufferFree(global_ctx, &buffer_ambi);
		if (buffer_stereo2.data)
			iplAudioBufferFree(global_ctx, &buffer_stereo2);
		if (buffer_stereo1.data)
			iplAudioBufferFree(global_ctx, &buffer_stereo1);
		if (buffer_input.data)
			iplAudioBufferFree(global_ctx, &buffer_input);
		if (buffer_mono.data)
			iplAudioBufferFree(global_ctx, &buffer_mono);
		if (ref_mixer)
			iplReflectionMixerRelease(&ref_mixer);
		if (ambi_effect)
			iplAmbisonicsDecodeEffectRelease(&ambi_effect);
		if (hrtf)
			iplHRTFRelease(&hrtf);
		if (simulator)
			iplSimulatorRelease(&simulator);
		if (static_mesh) {
			iplStaticMeshRemove(static_mesh, scene);
			iplSceneCommit(scene);
			iplStaticMeshRelease(&static_mesh);
		}
		if (scene)
			iplSceneRelease(&scene);
		if (embree_dev)
			iplEmbreeDeviceRelease(&embree_dev);
	}

	static void ipl_log(IPLLogLevel lvl, const char* msg) {
		console::formatter() << "Steam Audio: " << pfc::string(msg).replace("\n","");
	}

	// Sets up the SA Context
	bool setup_steamaudio_ctx() {
		if (!global_ctx) {
			console::formatter() << "Steam Audio: Creating Global CTX";
			IPLContextSettings cfg{};
			cfg.version = STEAMAUDIO_VERSION;
			cfg.logCallback = reinterpret_cast<IPLLogFunction>(dsp_steamaudio::ipl_log);
			if (iplContextCreate(&cfg, &global_ctx) != IPL_STATUS_SUCCESS)
				return false;
		} else {
			console::formatter() << "Steam Audio: Adding ref to Global CTX";
			iplContextRetain(global_ctx);
		}
		return true;
	}

	// Cleans up the SA Context
	void cleanup_steamaudio_ctx() {
		cleanup_steamaudio();
		console::formatter() << "Steam Audio: Cleaning up";
		if (global_ctx)
			iplContextRelease(&global_ctx);
	}

	// Other stuff

	dialogue_settings settings;
};

// Use dsp_factory_nopreset_t<> instead of dsp_factory_t<> if your DSP does not provide preset/configuration functionality.
static dsp_factory_t<dsp_steamaudio> g_dsp_steamaudio_factory;


class CSADSPPopup : public CDialogImpl<CSADSPPopup> {
public:
	CSADSPPopup(const dsp_preset & initData, dsp_preset_edit_callback & callback) : m_initData(initData), m_callback(callback) {}

	enum { IDD = IDD_DSP };

	enum {
		RangeMin = -60,
		RangeMax = 60,

		RangeTotal = RangeMax - RangeMin
	};

	BEGIN_MSG_MAP_EX(CSADSPPopup)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDOK, BN_CLICKED, OnButton)
		COMMAND_HANDLER_EX(IDAPPLY, BN_CLICKED, OnButton)
		COMMAND_HANDLER_EX(IDCANCEL, BN_CLICKED, OnButton)
		COMMAND_HANDLER_EX(IDHRTFBROWSE, BN_CLICKED, OnHRTFBrowse)
		COMMAND_CODE_HANDLER_EX(CBN_SELCHANGE, OnCombo)
		MSG_WM_HSCROLL(OnHScroll)
	END_MSG_MAP()

private:

	dialogue_settings settings;

	BOOL OnInitDialog(CWindow, LPARAM) {
		m_dark.AddDialogWithControls(m_hWnd);
		m_slider_input = GetDlgItem(IDC_SLIDER);
		m_slider_direct = GetDlgItem(IDC_SLIDER2);
		m_slider_ref = GetDlgItem(IDC_SLIDER3);
		m_slider_rsize = GetDlgItem(IDC_SLIDER4);
		m_combo_mat = GetDlgItem(IDC_COMBO1);
		m_edit_hrtf_file = GetDlgItem(IDC_EDIT1);

		for (int i=0;i<nummats;i++) {
			m_combo_mat.AddString(mat_names[i]);
		}
		m_slider_input.SetRange(0, RangeTotal);
		m_slider_direct.SetRange(0, RangeTotal);
		m_slider_ref.SetRange(0, RangeTotal);
		m_slider_rsize.SetRange(0, 100-10);
		m_edit_hrtf_file.SetLimitText(PATH_MAX_SIZE);

		{
			dsp_steamaudio::parse_preset(settings, m_initData);
		}

		m_slider_input.SetPos( pfc::clip_t<t_int32>( pfc::rint32(settings.input_gain), RangeMin, RangeMax ) - RangeMin );
		m_slider_direct.SetPos( pfc::clip_t<t_int32>( pfc::rint32(settings.direct_gain), RangeMin, RangeMax ) - RangeMin );
		m_slider_ref.SetPos( pfc::clip_t<t_int32>( pfc::rint32(settings.ref_gain), RangeMin, RangeMax ) - RangeMin );
		m_slider_rsize.SetPos( pfc::clip_t<t_int32>( pfc::rint32(settings.room_size*10.0f), 1, 100 ) - 10 );

		updateLabels();

		m_combo_mat.SetCurSel(settings.cubemat);

		m_edit_hrtf_file.SetWindowText(CString(settings.hrtf_file));

		return TRUE;
	}

	void updateLabels() {
		pfc::string_formatter msg;

		msg << pfc::format_float(settings.input_gain, 0, 2) << " dB";
		::uSetDlgItemText(*this, IDC_SLIDER_LABEL, msg);
		msg.clear();

		msg << pfc::format_float(settings.direct_gain, 0, 2) << " dB";
		::uSetDlgItemText(*this, IDC_SLIDER_LABEL2, msg);
		msg.clear();

		msg << pfc::format_float(settings.ref_gain, 0, 2) << " dB";
		::uSetDlgItemText(*this, IDC_SLIDER_LABEL3, msg);
		msg.clear();

		msg << pfc::format_float(settings.room_size, 0, 2);
		::uSetDlgItemText(*this, IDC_SLIDER_LABEL4, msg);
	}

	void onValueChange() {
		settings.input_gain = (float) ( m_slider_input.GetPos() + RangeMin );
		settings.direct_gain = (float) ( m_slider_direct.GetPos() + RangeMin );
		settings.ref_gain = (float) ( m_slider_ref.GetPos() + RangeMin );
		settings.room_size = ((float) ( m_slider_rsize.GetPos() + 10 ))/10.0f;
		settings.cubemat = m_combo_mat.GetCurSel();
		CString hrtf_file;
		m_edit_hrtf_file.GetWindowText(hrtf_file);
		CStringA hrtf_file_s = CStringA(hrtf_file);
		size_t size = (hrtf_file_s.GetLength() + 1);
		if (size > PATH_MAX_SIZE*2) {
			size = PATH_MAX_SIZE*2;
		}
		strcpy_s((char*)&settings.hrtf_file, size, hrtf_file_s);
		settings.hrtf_file[size] = '\x00';
	}

	void OnCombo(UINT, int id, CWindow) {
		onValueChange();
		//updatePreset();
	}

	void OnHRTFBrowse(UINT, int id, CWindow cwin) {
		wchar_t tmpstr[PATH_MAX_SIZE];
		OPENFILENAME open = { 0 };
		open.lStructSize = sizeof(OPENFILENAME);
		open.hwndOwner = cwin; //Handle to the parent window
		open.lpstrFilter = _T("SOFA Files(.sofa)\0*.sofa\0\0");
		open.lpstrCustomFilter = NULL;
		open.lpstrFile = (LPWSTR)&tmpstr;
		open.lpstrFile[0] = '\x00';
		open.nMaxFile = PATH_MAX_SIZE;
		open.nFilterIndex = 1;
		open.lpstrInitialDir = NULL;
		open.lpstrTitle = _T("Select An HRTF\0");
		open.nMaxFileTitle = strlen("Select an HRTF\0");
		open.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER;
		if (GetOpenFileName(&open)) {
			m_edit_hrtf_file.SetWindowText(CString(open.lpstrFile));
		}
	}

	void OnButton(UINT, int id, CWindow) {
		if (id == IDOK || id == IDAPPLY) {
			onValueChange();
			updatePreset();
		}
		if (id == IDOK || id == IDCANCEL)
			EndDialog(id);
	}

	void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar pScrollBar) {
		onValueChange();
		updateLabels();
		//updatePreset();
	}

	void updatePreset() {
		dsp_preset_impl preset;
		dsp_steamaudio::make_preset(settings, preset);
		m_callback.on_preset_changed(preset);
	}

	const dsp_preset & m_initData; // modal dialog so we can reference this caller-owned object.
	dsp_preset_edit_callback & m_callback;

	CComboBox m_combo_mat;
	CEdit m_edit_hrtf_file;
	CTrackBarCtrl m_slider_input;
	CTrackBarCtrl m_slider_direct;
	CTrackBarCtrl m_slider_ref;
	CTrackBarCtrl m_slider_rsize;
	fb2k::CDarkModeHooks m_dark;
};

static void RunDSPConfigPopup(const dsp_preset & p_data,HWND p_parent,dsp_preset_edit_callback & p_callback) {
	CSADSPPopup popup(p_data, p_callback);
	if (popup.DoModal(p_parent) != IDOK) {
		// If the dialog exited with something else than IDOK,k 
		// tell host that the editing has been cancelled by sending it old preset data that we got initialized with
		p_callback.on_preset_changed(p_data);
	}
}
