.PHONY: all

all:
	g++ -o ladspa.m.jack.synth synth.cc -I . ladspam-jack-0/synth.cc `pkg-config ladspam-0 ladspamm-0 jack --cflags --libs` -lladspam.pb -lprotobuf
