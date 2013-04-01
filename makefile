.PHONY: all

all:
	g++ -g -O3 -march=native -o ladspa.m.jack.synth synth.cc -I . ladspam-jack-0/synth.cc `pkg-config ladspam-0 ladspamm-0 jack --cflags --libs` -lladspam.pb -lprotobuf
	g++ -g -O3 -march=native -o ladspa.m.jack.instrument instrument.cc -I . ladspam-jack-0/instrument.cc ladspam-jack-0/synth.cc `pkg-config ladspam-0 ladspamm-0 jack --cflags --libs` -lladspam.pb -lprotobuf

