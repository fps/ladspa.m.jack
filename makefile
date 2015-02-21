.PHONY: all install

PREFIX ?= /usr/local

all: ladspa.m.jack.instrument ladspa.m.jack.synth

CXXFLAGS ?= -O3 -march=native -mfpmath=sse -DNDEBUG
#CXXFLAGS ?= -O0 -g -march=native -mfpmath=sse -DNDEBUG

ladspa.m.jack.instrument: instrument.cc ladspam-jack-0/instrument.h ladspam-jack-0/synth.h ladspam-jack-0/synth.cc
	g++ $(CXXFLAGS) -o ladspa.m.jack.instrument instrument.cc -I . ladspam-jack-0/instrument.cc ladspam-jack-0/synth.cc `pkg-config ladspam-0 ladspamm-0 jack --cflags --libs` -lladspam.pb -lprotobuf

ladspa.m.jack.synth: synth.cc ladspam-jack-0/instrument.h ladspam-jack-0/synth.h ladspam-jack-0/synth.cc
	g++ $(CXXFLAGS) -o ladspa.m.jack.synth synth.cc -I . ladspam-jack-0/synth.cc `pkg-config ladspam-0 ladspamm-0 jack --cflags --libs` -lladspam.pb -lprotobuf

 
install: all
	install -d $(PREFIX)/bin
	install ladspa.m.jack.instrument $(PREFIX)/bin
	install ladspa.m.jack.synth $(PREFIX)/bin
