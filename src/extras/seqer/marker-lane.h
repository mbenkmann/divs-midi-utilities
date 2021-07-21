#ifndef MARKER_LANE_INCLUDED
#define MARKER_LANE_INCLUDED

class MarkerLane;

#include "label-lane.h"
#include "midifile.h"
#include "window.h"

class MarkerLane: public LabelLane
{
public:
	MarkerLane(Window* window);
	void populateLabels();
	MidiFileEvent_t addEventAtXY(int x, int y);
	void moveEventsByXY(int x_offset, int y_offset);
};

#endif
