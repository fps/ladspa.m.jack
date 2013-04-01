#ifndef LADSPAM_JACK_SYNTH_HH
#define LADSPAM_JACK_SYNTH_HH

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
		
		int process(jack_nframes_t nframes)
		{
			return 0;
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
			
			return the_synth;
		}
		
		protected:
			ladspam::synth_ptr m_synth;
			unsigned m_control_period;
			jack_client_t *m_jack_client;
	};
}

#endif