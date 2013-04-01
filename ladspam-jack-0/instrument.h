#ifndef LADSPAM_JACK_INSTRUMENT_HH
#define LADSPAM_JACK_INSTRUMENT_HH

#include <sstream>

#include <ladspam-0/synth.h>
#include <ladspam.pb.h>

#include <jack/jack.h>
#include <jack/midiport.h>

#include <ladspam-jack-0/synth.h>

namespace ladspam_jack
{
	struct instrument : ladspam_jack::synth
	{
		instrument
		(
			const std::string& jack_client_name, 
			const ladspam_pb::Instrument& instrument_pb, 
			unsigned int control_period
		) :
			ladspam_jack::synth(jack_client_name, instrument_pb.synth(), control_period, false)
		{
			m_midi_in_jack_port = jack_port_register
			(
				m_jack_client, 
				"midi", 
				JACK_DEFAULT_MIDI_TYPE, 
				JackPortIsInput, 
				0
			);
			
			for (unsigned voice_index = 0; voice_index < instrument_pb.number_of_voices(); ++voice_index)
			{
				m_voices.push_back(voice_ptr(new voice(control_period)));
			}
			
			for (unsigned connection_index = 0; connection_index < instrument_pb.connections_size(); ++connection_index)
			{
				ladspam_pb::Connection connection = instrument_pb.connections(connection_index);
				
				m_synth->connect
				(
					connection.sink_index(),
					connection.sink_port_index(),
					m_voices[connection.source_index()]->m_port_buffers[connection.source_port_index()]
				);
			}
			
			activate();
		}
		
		~instrument()
		{
			jack_deactivate(m_jack_client);
			jack_client_close(m_jack_client);
		}
		
		virtual int process(jack_nframes_t nframes)
		{
			unsigned number_of_chunks = nframes/m_control_period;
			
			unsigned chunk_index = 0;
			
			void *midi_in_buffer = jack_port_get_buffer(m_midi_in_jack_port, nframes);
			
			jack_nframes_t event_count = jack_midi_get_event_count(midi_in_buffer);
			jack_nframes_t event_index = 0;
			
			unsigned sample_rate = jack_get_sample_rate(m_jack_client);
			
			unsigned last_frame_time = jack_last_frame_time(m_jack_client);
			
			for (unsigned frame_index = 0; frame_index < nframes; ++frame_index)
			{
				unsigned frame_in_chunk = frame_index % m_control_period;
				
				jack_midi_event_t midi_in_event;

				if (event_count > 0) 
				{
					jack_midi_event_get(&midi_in_event, midi_in_buffer, event_index);
				}
				
				for (unsigned voice_index = 0; voice_index < m_voices.size(); ++voice_index)
				{
					{
						ladspam::synth::buffer &buffer = *m_voices[voice_index]->m_port_buffers[0];
						buffer[frame_in_chunk] = 0.0;
					}
				}
				
				while (event_index < event_count && midi_in_event.time == frame_index) 
				{
					//cout << "event" << endl;
					
					if( ((*(midi_in_event.buffer) & 0xf0)) == 0x90 ) 	
					{
						/** note on */
						
						unsigned note = *(midi_in_event.buffer + 1);
						float velocity = *(midi_in_event.buffer + 2) / 128.0;
						
						int oldest_voice_index = oldest_voice(frame_index);

						m_voices[oldest_voice_index]->m_note = note;
						m_voices[oldest_voice_index]->m_gate = 1.0;
						m_voices[oldest_voice_index]->m_start_frame = last_frame_time + frame_index;
						m_voices[oldest_voice_index]->m_on_velocity = velocity;
						
						ladspam::synth::buffer &buffer = *m_voices[oldest_voice_index]->m_port_buffers[0];

						buffer[frame_in_chunk] = 1.0;
					}
					
					if( ((*(midi_in_event.buffer)) & 0xf0) == 0x80 ) 
					{
						/** note off */

						unsigned note = *(midi_in_event.buffer + 1);
						
						int voice_index = voice_playing_note(note);
						
						if (-1 != voice_index)
						{
							m_voices[voice_index]->m_gate = 0;
						}
					}
					
					++event_index;
					/**
					* TODO: Check return value for ENODATA
					*/
					jack_midi_event_get(&midi_in_event, midi_in_buffer, event_index);
				}

				for (unsigned voice_index = 0; voice_index < m_voices.size(); ++voice_index)
				{
					{
						ladspam::synth::buffer &buffer = *m_voices[voice_index]->m_port_buffers[1];
						buffer[frame_in_chunk] = m_voices[voice_index]->m_gate;
					}
					
					{
						ladspam::synth::buffer &buffer = *m_voices[voice_index]->m_port_buffers[2];
						buffer[frame_in_chunk] =  m_voices[voice_index]->m_on_velocity / 128.0;
					}
					
					{
						ladspam::synth::buffer &buffer = *m_voices[voice_index]->m_port_buffers[3];
						buffer[frame_in_chunk] =  note_frequency(m_voices[voice_index]->m_note);
					}
					
					{
						ladspam::synth::buffer &buffer = *m_voices[voice_index]->m_port_buffers[4];
						buffer[frame_in_chunk] = note_frequency(m_voices[voice_index]->m_note) / (float)sample_rate;
					}
				}
				
				
				if (0 == (frame_index - 1) % m_control_period)
				{
					process_chunk(nframes, chunk_index);
					++chunk_index;
				}
			}
			
			return 0;
		}
		
		protected:
			float note_frequency(unsigned int note) 
			{
				return (2.0 * 440.0 / 32.0) * pow(2, (((jack_default_audio_sample_t)note - 9.0) / 12.0));
			}

			unsigned oldest_voice(unsigned frame)
			{
				jack_nframes_t minimum_age = frame + jack_last_frame_time(m_jack_client) - m_voices[0]->m_start_frame;
				unsigned oldest_index = 0;
				
				for (unsigned voice_index = 1; voice_index < m_voices.size(); ++voice_index)
				{
					jack_nframes_t age = frame + jack_last_frame_time(m_jack_client) - m_voices[voice_index]->m_start_frame;
					if (age > minimum_age)
					{
						oldest_index = voice_index;
						minimum_age = age;
					}
				}
				
				return oldest_index;
			}
			
			int voice_playing_note(unsigned note)
			{
				for (unsigned voice_index = 0; voice_index < m_voices.size(); ++voice_index)
				{
					if (m_voices[voice_index]->m_note == note && m_voices[voice_index]->m_gate > 0)
					{
						return voice_index;
					}
				}
				
				// UGLY
				return -1;
			}

			struct voice
			{
				float m_gate;
				unsigned m_note;
				unsigned m_on_velocity;
				unsigned m_off_velocity;
				jack_nframes_t m_start_frame;
				std::vector<ladspam::synth::buffer_ptr> m_port_buffers;
				
				voice(unsigned control_period) :
					m_gate(0.0),
					m_note(0),
					m_on_velocity(0),
					m_off_velocity(0),
					m_start_frame(0)
				{
					{
						ladspam::synth::buffer_ptr buffer(new std::vector<float>());
						buffer->resize(control_period);
						m_port_buffers.push_back(buffer);
					}

					{
						ladspam::synth::buffer_ptr buffer(new std::vector<float>());
						buffer->resize(control_period);
						m_port_buffers.push_back(buffer);
					}

					{
						ladspam::synth::buffer_ptr buffer(new std::vector<float>());
						buffer->resize(control_period);
						m_port_buffers.push_back(buffer);
					}

					{
						ladspam::synth::buffer_ptr buffer(new std::vector<float>());
						buffer->resize(control_period);
						m_port_buffers.push_back(buffer);
					}

					{
						ladspam::synth::buffer_ptr buffer(new std::vector<float>());
						buffer->resize(control_period);
						m_port_buffers.push_back(buffer);
					}
				}
			};
			
			typedef boost::shared_ptr<voice> voice_ptr;

			std::vector<voice_ptr> m_voices;
			
			jack_port_t *m_midi_in_jack_port;
	};
}

#endif