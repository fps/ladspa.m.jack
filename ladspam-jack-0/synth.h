#ifndef LADSPAM_JACK_SYNTH_HH
#define LADSPAM_JACK_SYNTH_HH

#include <sstream>

#include <ladspam-0/synth.h>
#include <ladspam.pb.h>

#include <jack/jack.h>

namespace ladspam_jack
{
	struct synth
	{
		synth
		(
			const std::string &jack_client_name, 
			const ladspam_pb::Synth &synth_pb,
			unsigned control_period
		);
		
		~synth()
		{
			jack_deactivate(m_jack_client);
			jack_client_close(m_jack_client);
		}
		
		int process(jack_nframes_t nframes)
		{
			unsigned number_of_chunks = nframes/m_control_period;
			
			for (unsigned chunk_index = 0; chunk_index < number_of_chunks; ++chunk_index)
			{
				for (unsigned exposed_port_index = 0; exposed_port_index < m_jack_ports.size(); ++exposed_port_index)
				{
					float *buffer = (float*)jack_port_get_buffer(m_jack_ports[exposed_port_index], nframes);
					
					if (jack_port_flags(m_jack_ports[exposed_port_index]) & JackPortIsInput)
					{	
						std::copy
						(
							buffer + chunk_index * m_control_period,
							buffer + (chunk_index + 1) * m_control_period,
							m_exposed_plugin_port_buffers[exposed_port_index]->begin()
						);
					}
				}
				
				m_synth->process();
				
				for (unsigned exposed_port_index = 0; exposed_port_index < m_jack_ports.size(); ++exposed_port_index)
				{
					float *buffer = (float*)jack_port_get_buffer(m_jack_ports[exposed_port_index], nframes);
					
					if (jack_port_flags(m_jack_ports[exposed_port_index]) & JackPortIsOutput)
					{
						std::copy
						(
							m_exposed_plugin_port_buffers[exposed_port_index]->begin(),
							m_exposed_plugin_port_buffers[exposed_port_index]->end(),
							buffer + chunk_index * m_control_period
						);
					}
				}
			}
			
			return 0;
		}
		
		void expose_ports(const ladspam_pb::Synth &synth_pb, ladspam::synth_ptr the_synth)
		{
			for (unsigned port_index = 0; port_index < synth_pb.exposed_ports_size(); ++port_index)
			{
				ladspam_pb::Port port = synth_pb.exposed_ports(port_index);
				
				ladspamm::plugin_ptr the_plugin = the_synth->get_plugin(port.plugin_index())->the_plugin;
				
				unsigned long flags = 0;
				if (the_plugin->port_is_input(port.port_index()))
				{
					flags |= JackPortIsInput;
				}
				else
				{
					flags |= JackPortIsOutput;
				}
				
				std::stringstream port_name_stream;
				port_name_stream 
					<< port.plugin_index() 
					<< "-" << port.port_index() 
					<< "-" << the_plugin->label()
					<< "-" << the_plugin->port_name(port.port_index());
					
				jack_port_t *jack_port = jack_port_register(m_jack_client, port_name_stream.str().c_str(), JACK_DEFAULT_AUDIO_TYPE, flags, 0);
				
				m_jack_ports.push_back(jack_port);
				
				ladspam::synth::buffer_ptr buffer(new std::vector<float>);
				
				buffer->resize(m_control_period);
				
				m_exposed_plugin_port_buffers.push_back(buffer);
				
				the_synth->connect(port.plugin_index(), port.port_index(), buffer);
			}
		}
		
		ladspam::synth_ptr build_synth(const ladspam_pb::Synth& synth_pb, unsigned sample_rate, unsigned control_period)
		{
			ladspam::synth_ptr the_synth(new ladspam::synth(sample_rate, control_period));
			
			for (unsigned plugin_index = 0; plugin_index < synth_pb.plugins_size(); ++plugin_index)
			{
				ladspam_pb::Plugin plugin_pb = synth_pb.plugins(plugin_index);
				
				the_synth->append_plugin
				(
					plugin_pb.library(), 
					plugin_pb.label()
				);
			}
			
			for (unsigned connection_index = 0; connection_index < synth_pb.connections_size(); ++connection_index)
			{
				ladspam_pb::Connection connection_pb = synth_pb.connections(connection_index);
				
				the_synth->connect
				(
					connection_pb.source_plugin_index(),
					connection_pb.source_port_index(),
					connection_pb.sink_plugin_index(),
					connection_pb.sink_port_index()
				);
			}
			
			expose_ports(synth_pb, the_synth);
			
			return the_synth;
		}
		
		protected:
			ladspam::synth_ptr m_synth;
			
			unsigned m_control_period;
			
			jack_client_t *m_jack_client;
			
			std::vector<jack_port_t *> m_jack_ports;
			std::vector<ladspam::synth::buffer_ptr> m_exposed_plugin_port_buffers; 
	};
}

#endif