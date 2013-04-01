#include <ladspam-jack-0/synth.h>

namespace ladspam_jack
{
	extern "C"
	{
		int jack_process(jack_nframes_t nframes, void* arg)
		{
			return ((synth*)arg)->process(nframes);
		}
	}

	synth::synth
	(
		const std::string& jack_client_name, 
		const ladspam_pb::Synth& synth_pb, 
		unsigned int control_period
	) :
		m_control_period(control_period),
		m_jack_client(jack_client_open(jack_client_name.c_str(), JackUseExactName, 0))
	{
		if (NULL == m_jack_client)
		{
			throw std::runtime_error("Failed to open jack client");
		}
		
		m_synth = build_synth(synth_pb, jack_get_sample_rate(m_jack_client), m_control_period);
		
		if (0 != jack_set_process_callback(m_jack_client, jack_process, this))
		{
			throw std::runtime_error("Failed to set jack process callback");
		}
	
		if (0 != jack_get_buffer_size(m_jack_client) % m_control_period)
		{
			throw std::runtime_error("control period must be a divider of buffer size");
		}
		
		jack_activate(m_jack_client);
	}
}